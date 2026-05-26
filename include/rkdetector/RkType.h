#ifndef RKNN_YOLOV8_DEMO_RKTYPE_H
#define RKNN_YOLOV8_DEMO_RKTYPE_H

#include "rk_common.h"
#include "rknn_api.h"

#include <thread>
#include <sys/time.h>

#define OBJ_NUMB_MAX_SIZE 128

namespace rkdet
{

typedef struct
{
    float prop;
    int   cls_id;

    int   left;
    int   right;
    int   top;
    int   bottom;
} object_detect_result;

typedef struct
{
    int                  count;
    object_detect_result results[OBJ_NUMB_MAX_SIZE];
} object_detect_result_list;

static uint64_t getTimeStamp()
{
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return tv.tv_sec * 1000000 + tv.tv_usec;
}

} // namespace rkdet

#endif //RKNN_YOLOV8_DEMO_RKTYPE_H
