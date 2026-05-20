#include "RKDetector.h"
#include "FrameBuffer.h"
#include "log.h"

#include <atomic>
#include <thread>
#include <chrono>

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
                         const RKDetector&                detector)
{
    char text[256];
    for (int i = 0; i < results->count; i++)
    {
        const object_detect_result* det = &(results->results[i]);
        LOG_INFO("%s @ (%d %d %d %d) %.3f",
                 detector.cls_to_name(det->cls_id),
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
                detector.cls_to_name(det->cls_id),
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

    const char* model_path = argv[1];
    const char* image_path = argv[2];

    // Init detector
    RKDetector  detector;
    int         ret = detector.init(model_path, "./model/drone.txt");
    if (ret != 0)
    {
        LOG_ERROR("detector init fail! ret=%d", ret);
        return -1;
    }

    // Camera thread -> FrameBuffer
    FrameBuffer<cv::Mat> frame_buf;
    std::thread camera_thread(camera_thread_func, &frame_buf, image_path);

    // Inference loop
    int         frame_count = 0;
    while (g_running)
    {
        cv::Mat frame;
        if (!frame_buf.pop(frame))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        // BGR -> RGB for inference
        cv::Mat rgb;
        cv::cvtColor(frame, rgb, cv::COLOR_BGR2RGB);
        image_buffer_t            img = mat_to_buffer(rgb);

        auto                      t1  = getTimeStamp();
        object_detect_result_list results;
        ret = detector.detect(&img, &results, 0.45, 0.45);
        LOG_INFO("detect cost %f ms", (getTimeStamp() - t1) * 1e-3);

        if (ret != 0)
        {
            LOG_ERROR("detect fail! ret=%d", ret);
            continue;
        }

        // Draw results on original BGR frame
        draw_results(frame, &results, detector);
        cv::imwrite("out.png", frame);
        LOG_INFO("saved out.png");

        frame_count++;
        if (frame_count >= 1)
        {
            g_running = false;
        }
    }

    camera_thread.join();
    detector.release();

    return 0;
}
