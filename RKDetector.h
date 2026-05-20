#ifndef _RK_DETECTOR_H_
#define _RK_DETECTOR_H_

#include "NPUScheduler.h"
#include "common.h"
#include "RkType.h"

class RKDetector
{
public:
    int  init(const char* model_path, rknn_core_mask core_mask = RKNN_NPU_CORE_ALL);
    void release();
    int  detect(const image_buffer_t*      img,
                object_detect_result_list* results,
                float                      conf_threshold,
                float                      nms_threshold);

private:
    NPUScheduler npu_;
};

#endif /* _RK_DETECTOR_H_ */
