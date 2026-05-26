#ifndef _RKDETECTOR_NPU_DEVICE_POOL_H_
#define _RKDETECTOR_NPU_DEVICE_POOL_H_

#include "RKTypeConversion.h"
#include "rkdetector/RKDetector.h"
#include "rkdetector/NPULoadMonitor.h"
#include "rkdetector/log.h"

#include <array>
#include <condition_variable>
#include <mutex>
#include <vector>

namespace rkdet
{
struct TaskInfo
{
    uint64_t task_id;
    int      cam_id;
    int      frame_id;
    int      core_id;
    uint64_t start_time; // microseconds
};

// Per-core task snapshot
struct CoreTaskInfo
{
    int                   core_id;
    bool                  busy;
    std::vector<TaskInfo> tasks; // current task(s) on this core
};

template <int N>
class NPUDevicePool
{
public:
    int init(const char*           model_path,
             const char*           label_path,
             const rknn_core_mask* cores,
             NPULoadMonitor*       monitor = nullptr)
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_monitor_ptr = monitor;
        for (int i = 0; i < N; i++)
        {
            LOG_INFO("init detector[%d] on core %s",
                     i,
                     RKTypeConversion::RKMaskToString(cores[i]).c_str());
            int ret =
                m_detectors_array[i].init(model_path, label_path, cores[i]);
            if (ret != 0)
            {
                LOG_ERROR("detector[%d] init fail! ret=%d", i, ret);
                for (int j = i - 1; j >= 0; j--)
                {
                    m_detectors_array[j].release();
                }
                return -1;
            }
            m_busy_array[i] = false;
            m_core_tasks[i].clear();
        }
        m_last_used = -1;
        LOG_INFO("NPUDevicePool: %d detectors initialized", N);
        return 0;
    }

    void release()
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        for (int i = 0; i < N; i++)
        {
            m_detectors_array[i].release();
            m_busy_array[i] = false;
            m_core_tasks[i].clear();
        }
        m_last_used = -1;
        m_cv.notify_all();
    }

    // Acquire the best available detector.
    // Returns core index, records task with given cam_id.
    // Blocks if all cores are busy.
    int acquire(int cam_id, int frame_id)
    {
        std::unique_lock<std::mutex> lock(m_mtx);
        m_cv.wait(lock, [this] {
            for (int i = 0; i < N; i++)
            {
                if (!m_busy_array[i])
                {
                    return true;
                }
            }
            return false;
        });

        int      core = pick_best_locked();

        TaskInfo task{};
        task.task_id    = m_task_id_counter++;
        task.cam_id     = cam_id;
        task.frame_id   = frame_id;
        task.core_id    = core;
        task.start_time = getTimeStamp();
        m_core_tasks[core].push_back(task);

        return core;
    }

    void release_detector(int idx)
    {
        {
            std::lock_guard<std::mutex> lock(m_mtx);
            if (idx >= 0 && idx < N)
            {
                m_busy_array[idx] = false;
                // Remove oldest task for this core
                if (!m_core_tasks[idx].empty())
                {
                    m_core_tasks[idx].erase(m_core_tasks[idx].begin());
                }
            }
        }
        m_cv.notify_one();
    }

    // Get tasks for a specific core
    std::vector<TaskInfo> get_core_tasks(int core_id) const
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        if (core_id >= 0 && core_id < N)
        {
            return m_core_tasks[core_id];
        }
        return {};
    }

    // Get all cores' task info
    std::array<CoreTaskInfo, N> get_all_core_tasks() const
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        std::array<CoreTaskInfo, N> result;
        for (int i = 0; i < N; i++)
        {
            result[i].core_id = i;
            result[i].busy    = m_busy_array[i];
            result[i].tasks   = m_core_tasks[i];
        }
        return result;
    }

    // Get snapshot of all in-progress tasks (flat list)
    std::vector<TaskInfo> get_active_tasks() const
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        std::vector<TaskInfo>       all;
        for (int i = 0; i < N; i++)
        {
            all.insert(
                all.end(), m_core_tasks[i].begin(), m_core_tasks[i].end());
        }
        return all;
    }

    int active_task_count() const
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        int                         count = 0;
        for (int i = 0; i < N; i++)
        {
            count += (int)m_core_tasks[i].size();
        }
        return count;
    }

    RKDetector& detector(int idx)
    {
        return m_detectors_array[idx];
    }

private:
    int pick_best_locked()
    {
        int candidate[N];
        int n_candidates = 0;

        for (int i = 0; i < N; i++)
        {
            if (!m_busy_array[i] && i != m_last_used)
            {
                candidate[n_candidates++] = i;
            }
        }

        if (n_candidates == 0)
        {
            for (int i = 0; i < N; i++)
            {
                if (!m_busy_array[i])
                {
                    candidate[n_candidates++] = i;
                    break;
                }
            }
        }

        int filtered[N];
        int n_filtered = 0;

        if (m_monitor_ptr)
        {
            for (int i = 0; i < n_candidates; i++)
            {
                int load = m_monitor_ptr->get_core_load(candidate[i]);
                if (load <= 70)
                {
                    filtered[n_filtered++] = candidate[i];
                }
            }
        }

        int* pool;
        int  n_pool;
        if (n_filtered > 0)
        {
            pool   = filtered;
            n_pool = n_filtered;
        }
        else
        {
            pool   = candidate;
            n_pool = n_candidates;
        }

        int best = pool[0];
        for (int i = 0; i < n_pool; i++)
        {
            if (pool[i] > m_last_used)
            {
                best = pool[i];
                break;
            }
        }

        m_busy_array[best] = true;
        m_last_used        = best;
        return best;
    }

    std::array<RKDetector, N>            m_detectors_array;
    std::array<bool, N>                  m_busy_array = {};
    int                                  m_last_used  = -1;

    NPULoadMonitor*                      m_monitor_ptr = nullptr;

    std::array<std::vector<TaskInfo>, N> m_core_tasks; // per-core task list
    uint64_t                             m_task_id_counter{0};

    mutable std::mutex                   m_mtx;
    std::condition_variable              m_cv;
};
} // namespace rkdet

#endif /* _RKDETECTOR_NPU_DEVICE_POOL_H_ */
