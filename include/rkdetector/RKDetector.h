#ifndef _RKDETECTOR_RK_DETECTOR_H_
#define _RKDETECTOR_RK_DETECTOR_H_

#include "rkdetector/LabelTools.h"
#include "rkdetector/RKScheduler.h"
#include "rk_common.h"
#include "rkdetector/RkType.h"

namespace rkdet
{

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

} // namespace rkdet

#endif /* _RKDETECTOR_RK_DETECTOR_H_ */
