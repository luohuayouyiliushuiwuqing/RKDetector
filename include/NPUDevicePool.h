#ifndef _NPU_DEVICE_POOL_H_
#define _NPU_DEVICE_POOL_H_

#include "RKDetector.h"
#include "NPULoadMonitor.h"
#include "log.h"

#include <array>
#include <condition_variable>
#include <mutex>

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
        std::lock_guard<std::mutex> lock(mtx_);
        monitor_ = monitor;
        for (int i = 0; i < N; i++)
        {
            LOG_INFO("init detector[%d] on core %s",
                     i,
                     cov_rk2_string(cores[i]).c_str());
            int ret = detectors_[i].init(model_path, label_path, cores[i]);
            if (ret != 0)
            {
                LOG_ERROR("detector[%d] init fail! ret=%d", i, ret);
                for (int j = i - 1; j >= 0; j--)
                {
                    detectors_[j].release();
                }
                return -1;
            }
            busy_[i] = false;
        }
        last_used_ = -1;
        LOG_INFO("NPUDevicePool: %d detectors initialized", N);
        return 0;
    }

    void release()
    {
        std::lock_guard<std::mutex> lock(mtx_);
        for (int i = 0; i < N; i++)
        {
            detectors_[i].release();
            busy_[i] = false;
        }
        last_used_ = -1;
        cv_.notify_all();
    }

    // Acquire the best available detector:
    //   1. Among free cores, avoid last_used_ to spread load
    //   2. Among candidates, pick lowest NPU load from monitor
    //   3. If no monitor, round-robin among free cores
    // Blocks if all cores are busy.
    int acquire()
    {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [this] {
            for (int i = 0; i < N; i++)
            {
                if (!busy_[i])
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
            std::lock_guard<std::mutex> lock(mtx_);
            if (idx >= 0 && idx < N)
            {
                busy_[idx] = false;
            }
        }
        cv_.notify_one();
    }

    RKDetector& detector(int idx)
    {
        return detectors_[idx];
    }

private:
    // Must be called with mtx_ held.
    // Strategy: round-robin among free cores (skip last_used_).
    // Only consult monitor to avoid cores with very high external load (>70%).
    int pick_best_locked()
    {
        // Log current busy state
        // LOG_DEBUG("pick: busy=[%d,%d,%d] last_used=%d",
        //           busy_[0], busy_[1], busy_[2], last_used_);

        // Build list of free cores, excluding last_used_
        int candidate[N];
        int n_candidates = 0;

        for (int i = 0; i < N; i++)
        {
            if (!busy_[i] && i != last_used_)
            {
                candidate[n_candidates++] = i;
            }
        }

        // If only last_used_ is free, use it
        if (n_candidates == 0)
        {
            for (int i = 0; i < N; i++)
            {
                if (!busy_[i])
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

        if (monitor_)
        {
            for (int i = 0; i < n_candidates; i++)
            {
                int load = monitor_->get_core_load(candidate[i]);
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
            if (pool[i] > last_used_)
            {
                best = pool[i];
                break;
            }
        }

        busy_[best] = true;
        last_used_  = best;
        // LOG_DEBUG("pick: -> core %d", best);
        return best;
    }

    std::array<RKDetector, N> detectors_;
    std::array<bool, N>       busy_{};
    int                       last_used_{-1};
    NPULoadMonitor*           monitor_{};

    std::mutex                mtx_;
    std::condition_variable   cv_;
};

#endif /* _NPU_DEVICE_POOL_H_ */
