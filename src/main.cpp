#include "NPUDevicePool.h"
#include "FrameBuffer.h"
#include "LabelTools.h"
#include "NPULoadMonitor.h"
#include "../include/log.h"

#include <atomic>
#include <thread>
#include <chrono>
#include <vector>

#include <opencv2/opencv.hpp>

static std::atomic<bool> g_running{true};

static image_buffer_t    mat_to_buffer(const cv::Mat& mat)
{
    image_buffer_t buf;
    memset(&buf, 0, sizeof(buf));
    buf.width     = mat.cols;
    buf.height    = mat.rows;
    buf.format    = IMAGE_FORMAT_RGB888;
    buf.virt_addr = mat.data;
    buf.size      = (int)(mat.total() * mat.elemSize());
    return buf;
}

static void camera_thread_func(FrameBuffer<cv::Mat>* fb, const char* image_path)
{
    cv::Mat img = cv::imread(image_path, cv::IMREAD_COLOR);
    if (img.empty())
    {
        LOG_ERROR("camera: read image fail! path=%s", image_path);
        g_running = false;
        return;
    }
    LOG_INFO("camera: loaded %s (%dx%d)", image_path, img.cols, img.rows);

    while (g_running)
    {
        fb->push(img);
        std::this_thread::sleep_for(std::chrono::milliseconds(33));
    }
    LOG_INFO("camera: thread exit");
}

static void draw_results(cv::Mat&                         bgr_img,
                         const object_detect_result_list* results,
                         const LabelTools&                label_tools)
{
    char text[256];
    for (int i = 0; i < results->count; i++)
    {
        const object_detect_result* det = &(results->results[i]);
        LOG_INFO("%s @ (%d %d %d %d) %.3f",
                 label_tools.get_name(det->cls_id),
                 det->box.left,
                 det->box.top,
                 det->box.right,
                 det->box.bottom,
                 det->prop);

        cv::rectangle(bgr_img,
                      cv::Point(det->box.left, det->box.top),
                      cv::Point(det->box.right, det->box.bottom),
                      cv::Scalar(255, 0, 0),
                      3);

        sprintf(text,
                "%s %.1f%%",
                label_tools.get_name(det->cls_id),
                det->prop * 100);
        cv::putText(bgr_img,
                    text,
                    cv::Point(det->box.left, det->box.top - 5),
                    cv::FONT_HERSHEY_SIMPLEX,
                    0.5,
                    cv::Scalar(0, 0, 255),
                    1);
    }
}

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        LOG_ERROR("usage: %s <model_path> <image_path>", argv[0]);
        return -1;
    }

    const char*          model_path = argv[1];
    const char*          image_path = argv[2];

    // Init detector pool (one per NPU core)
    const rknn_core_mask cores[]    = {
        RKNN_NPU_CORE_0,
        RKNN_NPU_CORE_1,
        RKNN_NPU_CORE_2,
    };
    NPUDevicePool<3> pool;
    LabelTools       label_tools("./model/drone.txt");
    NPULoadMonitor   monitor;
    std::thread      monitor_thread([&monitor] {
        monitor.start(50000);
    }); // 10ms

    int              ret = pool.init(model_path, cores, &monitor);
    if (ret != 0)
    {
        LOG_ERROR("pool init fail! ret=%d", ret);
        monitor.stop();
        monitor_thread.join();
        return -1;
    }

    // Camera thread -> FrameBuffer
    FrameBuffer<cv::Mat> frame_buf;
    std::thread camera_thread(camera_thread_func, &frame_buf, image_path);

    // Inference loop — async dispatch, don't block on detect
    std::vector<std::thread> workers;
    std::atomic<int>         frame_count{0};
    const int                max_frames = 1200;

    while (g_running)
    {
        cv::Mat frame;
        if (!frame_buf.pop(frame))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        // Fire detect asynchronously
        workers.emplace_back([&pool,
                              &label_tools,
                              &frame_count,
                              max_frames,
                              frame = std::move(frame)]() mutable {
            int dev = pool.acquire();
            auto t1 = getTimeStamp();

            object_detect_result_list results;
            int ret = -1;

            // BGR -> RGB
            cv::Mat rgb;
            cv::cvtColor(frame, rgb, cv::COLOR_BGR2RGB);
            image_buffer_t img = mat_to_buffer(rgb);

            ret = pool.detector(dev).detect(&img, &results, 0.45, 0.45);

            pool.release_detector(dev);
            LOG_INFO("detect cost %f ms (core %d)",
                     (getTimeStamp() - t1) * 1e-3,
                     dev);

            if (ret == 0)
            {
                // cv::Mat draw_img = frame.clone();
                //
                // draw_results(draw_img, &results, label_tools);
                // cv::imwrite("out.png", draw_img);
                // LOG_INFO("saved out.png");
            }
            else
            {
                LOG_ERROR("detect fail! ret=%d", ret);
            }

            if (frame_count.fetch_add(1) + 1 >= max_frames)
            {
                g_running = false;
            }
        });
    }

    // Wait for all in-flight detections to finish
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
    label_tools.release();

    return 0;
}
