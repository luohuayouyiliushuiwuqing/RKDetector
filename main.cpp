#include "inference.h"
#include "log.h"

#include <stdio.h>
#include <string.h>

#include <opencv2/opencv.hpp>

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        LOG_ERROR("usage: %s <model_path> <image_path>", argv[0]);
        return -1;
    }

    const char*        model_path = argv[1];
    const char*        image_path = argv[2];

    int                ret;
    rknn_app_context_t rknn_app_ctx;
    memset(&rknn_app_ctx, 0, sizeof(rknn_app_context_t));

    std::string file_path = "./model/drone.txt";

    init_post_process(file_path.c_str());

    ret = init_yolo_model(model_path, &rknn_app_ctx);
    if (ret != 0)
    {
        LOG_ERROR("init model fail! ret=%d model_path=%s", ret, model_path);
        deinit_post_process();
        return -1;
    }

    cv::Mat bgr_img = cv::imread(image_path, cv::IMREAD_COLOR);
    if (bgr_img.empty())
    {
        LOG_ERROR("read image fail! image_path=%s", image_path);
        release_yolo_model(&rknn_app_ctx);
        deinit_post_process();
        return -1;
    }

    // Convert BGR to RGB for inference
    cv::Mat rgb_img;
    cv::cvtColor(bgr_img, rgb_img, cv::COLOR_BGR2RGB);

    // Fill image_buffer_t from cv::Mat
    image_buffer_t src_image;
    memset(&src_image, 0, sizeof(image_buffer_t));
    src_image.width     = rgb_img.cols;
    src_image.height    = rgb_img.rows;
    src_image.format    = IMAGE_FORMAT_RGB888;
    src_image.virt_addr = rgb_img.data;
    src_image.size      = rgb_img.total() * rgb_img.elemSize();

    object_detect_result_list od_results;

    auto                      t1 = getTimeStamp();
    ret = inference_model(&rknn_app_ctx, &src_image, &od_results);
    LOG_INFO("inference_model cost %f ms", (getTimeStamp() - t1) * 1e-3);
    if (ret != 0)
    {
        LOG_ERROR("inference model fail! ret=%d", ret);
        release_yolo_model(&rknn_app_ctx);
        deinit_post_process();
        return -1;
    }

    // Draw results with OpenCV
    char text[256];
    for (int i = 0; i < od_results.count; i++)
    {
        object_detect_result* det_result = &(od_results.results[i]);
        LOG_INFO("%s @ (%d %d %d %d) %.3f",
                 coco_cls_to_name(det_result->cls_id),
                 det_result->box.left,
                 det_result->box.top,
                 det_result->box.right,
                 det_result->box.bottom,
                 det_result->prop);

        int x1 = det_result->box.left;
        int y1 = det_result->box.top;
        int x2 = det_result->box.right;
        int y2 = det_result->box.bottom;

        // Draw bounding box (blue)
        cv::rectangle(bgr_img,
                      cv::Point(x1, y1),
                      cv::Point(x2, y2),
                      cv::Scalar(255, 0, 0),
                      3);

        // Draw label text (red)
        sprintf(text,
                "%s %.1f%%",
                coco_cls_to_name(det_result->cls_id),
                det_result->prop * 100);
        cv::putText(bgr_img,
                    text,
                    cv::Point(x1, y1 - 5),
                    cv::FONT_HERSHEY_SIMPLEX,
                    0.5,
                    cv::Scalar(0, 0, 255),
                    1);
    }

    cv::imwrite("out.png", bgr_img);

    release_yolo_model(&rknn_app_ctx);
    deinit_post_process();

    return 0;
}
