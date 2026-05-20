#ifndef _NPU_DEVICE_POOL_H_
#define _NPU_DEVICE_POOL_H_

#include "RKDetector.h"
#include "log.h"

#include <array>
#include <condition_variable>
#include <mutex>

template<int N>
class NPUDevicePool
{
public:
    int init(const char* model_path, const rknn_core_mask* cores)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        for (int i = 0; i < N; i++)
        {
            LOG_INFO("init detector[%d] on core %d", i, cores[i]);
            int ret = detectors_[i].init(model_path, cores[i]);
            if (ret != 0)
            {
                LOG_ERROR("detector[%d] init fail! ret=%d", i, ret);
                // Release already-initialized detectors
                for (int j = i - 1; j >= 0; j--)
                {
                    detectors_[j].release();
                }
                return -1;
            }
            busy_[i] = false;
        }
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
        cv_.notify_all();
    }

    // Acquire the least-busy detector. Blocks if all are busy.
    // Returns index [0, N-1].
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

        // Pick the first free detector
        for (int i = 0; i < N; i++)
        {
            if (!busy_[i])
            {
                busy_[i] = true;
                return i;
            }
        }
        // Unreachable
        return -1;
    }

    // Release a previously acquired detector.
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
    std::array<RKDetector, N> detectors_;
    std::array<bool, N>       busy_{};
    std::mutex                mtx_;
    std::condition_variable   cv_;
};

#endif /* _NPU_DEVICE_POOL_H_ */
