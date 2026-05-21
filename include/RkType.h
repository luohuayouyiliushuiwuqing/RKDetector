#ifndef RKNN_YOLOV8_DEMO_RKTYPE_H
#define RKNN_YOLOV8_DEMO_RKTYPE_H

#include "../utils/common.h"
#include "rknn_api.h"

#include <sys/time.h>

typedef struct
{
    rknn_context          rknn_ctx;
    rknn_input_output_num io_num;
    rknn_tensor_attr*     input_attrs;
    rknn_tensor_attr*     output_attrs;
    int                   model_channel;
    int                   model_width;
    int                   model_height;
    int                   obj_class_num;
    bool                  is_quant;
} rknn_app_context_t;

#define OBJ_NUMB_MAX_SIZE 128
#define NMS_THRESH        0.45
#define BOX_THRESH        0.25

typedef struct
{
    image_rect_t box;
    float        prop;
    int          cls_id;
} object_detect_result;

typedef struct
{
    int                  id;
    int                  count;
    object_detect_result results[OBJ_NUMB_MAX_SIZE];
} object_detect_result_list;

static uint64_t getTimeStamp()
{
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return tv.tv_sec * 1000000 + tv.tv_usec;
}
#endif //RKNN_YOLOV8_DEMO_RKTYPE_H
