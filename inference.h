#ifndef _RKNN_DEMO_INFERENCE_H_
#define _RKNN_DEMO_INFERENCE_H_

#include "RkType.h"
#include "common.h"

#include "postprocess_common.h"

int init_yolo_model(const char* model_path, rknn_app_context_t* app_ctx);

int release_yolo_model(rknn_app_context_t* app_ctx);

int inference_model(rknn_app_context_t*        app_ctx,
                    image_buffer_t*            img,
                    object_detect_result_list* od_results);

#endif //_RKNN_DEMO_INFERENCE_H_
