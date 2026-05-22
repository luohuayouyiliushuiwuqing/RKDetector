#include "NPUDevicePool.h"
#include "LabelTools.h"
#include "NPULoadMonitor.h"
#include "V4L2Camera.h"
#include "include/log.h"
#include "image_utils.h"

#include <atomic>
#include <thread>
#include <mutex>
#include <vector>
#include <cstring>

static std::atomic<bool> g_running{true};

static void              camera_thread_func(const char*               dev_path,
                                            int                       cam_id,
                                            NPUDevicePool<3>*         pool,
                                            std::atomic<int>*         frame_count,
                                            std::vector<std::thread>* workers,
                                            std::mutex*               workers_mtx)
{
    V4L2Camera cap;
    if (!cap.open(dev_path))
    {
        LOG_ERROR("camera[%d]: open %s fail!", cam_id, dev_path);
        return;
    }
    LOG_INFO("camera[%d]: opened %s", cam_id, dev_path);

    while (g_running)
    {
        V4L2Frame nv12;
        if (!cap.read(nv12))
        {
            LOG_WARN("camera[%d]: read fail, skip", cam_id);
            continue;
        }

        // Copy NV12 data — buffer will be re-queued immediately
        uint32_t nv12_size = nv12.width * nv12.height * 3 / 2;
        uint8_t* nv12_copy = (uint8_t*)malloc(nv12_size);
        memcpy(nv12_copy, nv12.data, nv12_size);

        {
            std::lock_guard<std::mutex> lock(*workers_mtx);
            workers->emplace_back([pool,
                                   frame_count,
                                   cam_id,
                                   nv12_copy,
                                   nv12_size,
                                   w = nv12.width,
                                   h = nv12.height]() {
                int      dev      = pool->acquire();
                auto     t1       = getTimeStamp();

                // RGA hardware: NV12 → RGB888
                uint32_t rgb_size = w * h * 3;
                uint8_t* rgb_buf  = (uint8_t*)malloc(rgb_size);
                cvtColor(nv12_copy,
                         IMAGE_FORMAT_YUV420SP_NV12,
                         w,
                         h,
                         rgb_buf,
                         IMAGE_FORMAT_RGB888,
                         w,
                         h);

                image_buffer_t img{};
                img.width     = w;
                img.height    = h;
                img.format    = IMAGE_FORMAT_RGB888;
                img.virt_addr = rgb_buf;
                img.size      = rgb_size;

                object_detect_result_list results;
                int                       ret =
                    pool->detector(dev).detect(&img, &results, 0.45, 0.45);

                pool->release_detector(dev);
                LOG_INFO("cam[%d] detect cost %.1f ms (core %d)",
                         cam_id,
                         (getTimeStamp() - t1) * 1e-3,
                         dev);

                free(rgb_buf);
                free(nv12_copy);

                if (ret != 0)
                {
                    LOG_ERROR("cam[%d] detect fail! ret=%d", cam_id, ret);
                }
            });
        }
    }
    LOG_INFO("camera[%d]: thread exit", cam_id);
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        LOG_ERROR("usage: %s <model_path>", argv[0]);
        return -1;
    }

    const char* model_path = argv[1];

    const char* label_path = "model/drone.txt";

    const char* cameras[]  = {
        "/dev/mipi_camera_0_main",
        "/dev/mipi_camera_1_main",
    };

    // Init detector pool (one per NPU core)
    const rknn_core_mask cores[] = {
        RKNN_NPU_CORE_0,
        RKNN_NPU_CORE_1,
        RKNN_NPU_CORE_2,
    };
    NPUDevicePool<3> pool;
    NPULoadMonitor   monitor;
    std::thread      monitor_thread([&monitor] {
        monitor.start(50);
    });

    int              ret = pool.init(model_path, label_path, cores, &monitor);
    if (ret != 0)
    {
        LOG_ERROR("pool init fail! ret=%d", ret);
        monitor.stop();
        monitor_thread.join();
        return -1;
    }

    // Shared workers container
    std::vector<std::thread> workers;
    std::mutex               workers_mtx;
    std::atomic<int>         frame_count{0};

    // Launch two camera threads — both feed into the same pool
    std::thread              cam0(camera_thread_func,
                     cameras[0],
                     0,
                     &pool,
                     &frame_count,
                     &workers,
                     &workers_mtx);
    std::thread              cam1(camera_thread_func,
                     cameras[1],
                     1,
                     &pool,
                     &frame_count,
                     &workers,
                     &workers_mtx);

    cam0.join();
    cam1.join();

    for (auto& t : workers)
    {
        if (t.joinable())
        {
            t.join();
        }
    }

    monitor.stop();
    monitor_thread.join();
    pool.release();

    return 0;
}
