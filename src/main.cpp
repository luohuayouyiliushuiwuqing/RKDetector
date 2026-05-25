#include "rkdetector/NPUDevicePool.h"
#include "rkdetector/NPULoadMonitor.h"
#include "rkdetector/TaskQueue.h"
#include "rkdetector/V4L2Camera.h"
#include "rkdetector/log.h"
#include "rk_image_utils.h"

#include <atomic>
#include <thread>
#include <mutex>
#include <vector>
#include <cstring>

using namespace rkdet;

static std::atomic<bool> g_running{true};

using NPUPool = NPUDevicePool<3>;

static void camera_thread_func(const char* dev_path,
                               int         cam_id,
                               NPUPool*    pool,
                               TaskQueue*  task_queue)
{
    V4L2Camera cap;
    if (!cap.open(dev_path))
    {
        LOG_ERROR("camera[%d]: open %s fail!", cam_id, dev_path);
        return;
    }
    LOG_INFO("camera[%d]: opened %s", cam_id, dev_path);

    int frame_id = 0;

    while (g_running)
    {
        V4L2Frame nv12;
        if (!cap.read(nv12))
        {
            LOG_WARN("camera[%d]: read fail, skip", cam_id);
            continue;
        }

        // Copy NV12 — V4L2 buffer re-queued immediately
        uint32_t nv12_size = nv12.width * nv12.height * 3 / 2;
        uint8_t* nv12_copy = (uint8_t*)malloc(nv12_size);
        memcpy(nv12_copy, nv12.data, nv12_size);

        task_queue->push([pool,
                          cam_id,
                          frame_id,
                          nv12_copy,
                          nv12_size,
                          w = nv12.width,
                          h = nv12.height] {
            int      dev      = pool->acquire(cam_id, frame_id);
            auto     t1       = getTimeStamp();

            // RGA hardware: NV12 → RGB888
            uint32_t rgb_size = w * h * 3;
            uint8_t* rgb_buf  = (uint8_t*)malloc(rgb_size);
            cvtColor(nv12_copy,
                     IMAGE_FORMAT_YUV420SP_NV12,
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
            int ret = pool->detector(dev).detect(&img, &results, 0.45, 0.45);

            pool->release_detector(dev);
            LOG_DEBUG("cam[%d] frame[%d] detect cost %.1f ms (core %d)",
                      cam_id,
                      frame_id,
                      (getTimeStamp() - t1) * 1e-3,
                      dev);

            LOG_DEBUG("========================");

            auto all = pool->get_all_core_tasks();
            for (auto& core : all)
            {
                LOG_DEBUG("core=%d: busy=%d tasks_size=%zu",
                          core.core_id,
                          core.busy,
                          core.tasks.size());
                for (auto& t : core.tasks)
                {
                    LOG_DEBUG("    core=%d,task=%lu: cam=%d frame=%d time=%lu ",
                              core.core_id,
                              t.task_id,
                              t.cam_id,
                              t.frame_id,
                              t.start_time);
                }
            }

            LOG_DEBUG("****************************");

            free(rgb_buf);
            free(nv12_copy);

            if (ret != 0)
            {
                LOG_ERROR("cam[%d] detect fail! ret=%d", cam_id, ret);
            }
        });

        frame_id++;
    }
    LOG_INFO("camera[%d]: thread exit", cam_id);
}

static void worker_func(TaskQueue* task_queue, int worker_id)
{
    while (true)
    {
        TaskQueue::Task task;
        if (!task_queue->pop(task))
        {
            break; // stopped
        }
        task();
    }
    LOG_INFO("worker[%d]: exit", worker_id);
}

int main(int argc, char** argv)
{
    // if (argc != 2)
    // {
    //     LOG_ERROR("usage: %s <model_path>", argv[0]);
    //     return -1;
    // }
    const char*          model_path = "model/v8.rknn";

    // Init detector pool (one per NPU core)
    const rknn_core_mask cores[]    = {
        RKNN_NPU_CORE_0,
        RKNN_NPU_CORE_1,
        RKNN_NPU_CORE_2,
    };
    NPUPool        pool;
    NPULoadMonitor monitor;
    std::thread    monitor_thread([&monitor] {
        monitor.start(50);
    });

    int ret = pool.init(model_path, "model/drone.txt", cores, &monitor);
    if (ret != 0)
    {
        LOG_ERROR("pool init fail! ret=%d", ret);
        monitor.stop();
        monitor_thread.join();
        return -1;
    }

    // Fixed worker thread pool — 6 workers for 3 NPU cores
    // (2 per core to hide RGA + memcpy latency)
    TaskQueue                task_queue(32);
    const int                num_workers = 3;
    std::vector<std::thread> workers;
    workers.reserve(num_workers);
    for (int i = 0; i < num_workers; i++)
    {
        workers.emplace_back(worker_func, &task_queue, i);
    }

    // Launch two camera threads
    std::thread cam0(
        camera_thread_func, "/dev/mipi_camera_0_main", 0, &pool, &task_queue);
    std::thread cam1(
        camera_thread_func, "/dev/mipi_camera_1_main", 1, &pool, &task_queue);
    // std::thread cam2(
    //     camera_thread_func, "/dev/mipi_camera_2_main", 2, &pool, &task_queue);
    // std::thread cam3(
    //     camera_thread_func, "/dev/mipi_camera_0_save", 3, &pool, &task_queue);
    // std::thread cam4(
    //     camera_thread_func, "/dev/mipi_camera_1_save", 4, &pool, &task_queue);
    // std::thread cam5(
    //     camera_thread_func, "/dev/mipi_camera_2_save", 5, &pool, &task_queue);

    cam0.join();
    cam1.join();
    // cam2.join();
    // cam3.join();
    // cam4.join();
    // cam5.join();

    // Stop task queue and wait for workers
    task_queue.stop();
    for (auto& t : workers)
    {
        if (t.joinable())
        {
            t.join();
        }
    }

    if (task_queue.drop_count() > 0)
    {
        LOG_WARN("task_queue: %lu frames dropped (queue full)",
                 task_queue.drop_count());
    }

    monitor.stop();
    monitor_thread.join();
    pool.release();

    return 0;
}
