#pragma once
#ifndef RKNN_YOLOV8_DEMO_YOLO_H
#define RKNN_YOLOV8_DEMO_YOLO_H
#include "RkType.h"

int init_yolo_model(const char* model_path, rknn_app_context_t* app_ctx);

int release_yolo_model(rknn_app_context_t* app_ctx);

#endif //RKNN_YOLOV8_DEMO_YOLO_H
