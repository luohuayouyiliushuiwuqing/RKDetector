#ifndef _RKNN_DEMO_YOLOV10_H_
#define _RKNN_DEMO_YOLOV10_H_

#include "RkType.h"
#include "common.h"

#include "postprocess.h"

int inference_yolo26_model(rknn_app_context_t*        app_ctx,
                           image_buffer_t*            img,
                           object_detect_result_list* od_results);

#endif //_RKNN_DEMO_YOLOV10_H_