#ifndef _RK_DETECTOR_H_
#define _RK_DETECTOR_H_

#include "LabelTools.h"
#include "RKScheduler.h"
#include "common.h"
#include "RkType.h"

class RKDetector
{
public:
    int  init(const char*    model_path,
              const char*    label_path,
              rknn_core_mask core_mask = RKNN_NPU_CORE_ALL);
    void release();
    int  detect(const image_buffer_t*      img,
                object_detect_result_list* results,
                float                      conf_threshold,
                float                      nms_threshold);

private:
    RKScheduler m_rk_scheduler;
    LabelTools  m_label_tools;
};

#endif /* _RK_DETECTOR_H_ */
