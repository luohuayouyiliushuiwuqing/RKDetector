#ifndef _RKDETECTOR_NPU_DEVICE_POOL_H_
#define _RKDETECTOR_NPU_DEVICE_POOL_H_

#include "rkdetector/RKDetector.h"
#include "rkdetector/NPULoadMonitor.h"
#include "rkdetector/log.h"

#include <array>
#include <condition_variable>
#include <mutex>

namespace rkdet
{

inline std::string cov_rk2_string(rknn_core_mask core_mask)
{
    switch (core_mask)
    {
    case RKNN_NPU_CORE_AUTO:
        return "Core AUTO";
    case RKNN_NPU_CORE_0:
        return "Core 0";
    case RKNN_NPU_CORE_1:
        return "Core 1";
    case RKNN_NPU_CORE_2:
        return "Core 2";
    case RKNN_NPU_CORE_0_1:
        return "Core 0_1";
    case RKNN_NPU_CORE_0_1_2:
        return "Core 0_1_2";
    case RKNN_NPU_CORE_ALL:
        return "Core ALL";
    case RKNN_NPU_CORE_UNDEFINED:
        return "UNCore DEFINED";
    default:
        return "UNKNOWN";
    }
}

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
        m_monitor_ptr = std::shared_ptr<NPULoadMonitor>(monitor);
        for (int i = 0; i < N; i++)
        {
            LOG_INFO("init detector[%d] on core %s",
                     i,
                     cov_rk2_string(cores[i]).c_str());
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
        }
        m_last_used = -1;
        m_cv.notify_all();
    }

    // Acquire the best available detector:
    //   1. Among free cores, avoid last_used_ to spread load
    //   2. Among candidates, pick lowest NPU load from monitor
    //   3. If no monitor, round-robin among free cores
    // Blocks if all cores are busy.
    int acquire()
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

        return pick_best_locked();
    }

    void release_detector(int idx)
    {
        {
            std::lock_guard<std::mutex> lock(m_mtx);
            if (idx >= 0 && idx < N)
            {
                m_busy_array[idx] = false;
            }
        }
        m_cv.notify_one();
    }

    RKDetector& detector(int idx)
    {
        return m_detectors_array[idx];
    }

private:
    // Must be called with m_mtx held.
    // Strategy: round-robin among free cores (skip last_used_).
    // Only consult monitor to avoid cores with very high external load (>70%).
    int pick_best_locked()
    {
        // Log current busy state
        // LOG_DEBUG("pick: busy=[%d,%d,%d] last_used=%d",
        //           m_busy_array_[0], m_busy_array_[1], m_busy_array_[2], last_used_);

        // Build list of free cores, excluding last_used_
        int candidate[N];
        int n_candidates = 0;

        for (int i = 0; i < N; i++)
        {
            if (!m_busy_array[i] && i != m_last_used)
            {
                candidate[n_candidates++] = i;
            }
        }

        // If only last_used_ is free, use it
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

        // Log candidates
        {
            char buf[32];
            int  off = 0;
            for (int i = 0; i < n_candidates; i++)
            {
                off +=
                    snprintf(buf + off, sizeof(buf) - off, "%d ", candidate[i]);
            }
            // LOG_DEBUG("pick: candidates=[%s]", buf);
        }

        // Filter out cores with very high external load (>70%)
        int filtered[N];
        int n_filtered = 0;

        if (m_monitor_ptr)
        {
            for (int i = 0; i < n_candidates; i++)
            {
                int load = m_monitor_ptr->get_core_load(candidate[i]);
                // LOG_DEBUG("pick: core %d sys_load=%d", candidate[i], load);
                if (load <= 70)
                {
                    filtered[n_filtered++] = candidate[i];
                }
            }
        }

        // If all candidates are heavily loaded, use original list
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

        // Round-robin: pick the one after last_used_ in circular order
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
        // LOG_DEBUG("pick: -> core %d", best);
        return best;
    }

    std::array<RKDetector, N>       m_detectors_array;
    std::array<bool, N>             m_busy_array = {};
    int                             m_last_used  = -1;

    std::shared_ptr<NPULoadMonitor> m_monitor_ptr;

    std::mutex                      m_mtx;
    std::condition_variable         m_cv;
};

} // namespace rkdet

#endif /* _RKDETECTOR_NPU_DEVICE_POOL_H_ */
