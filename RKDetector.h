#ifndef _RK_DETECTOR_H_
#define _RK_DETECTOR_H_

#include "NPUScheduler.h"
#include "common.h"
#include "RkType.h"

class RKDetector
{
public:
    int   init(const char* model_path, const char* label_path);
    void  release();
    int   detect(const image_buffer_t*      img,
                 object_detect_result_list* results,
                 float                      conf_threshold,
                 float                      nms_threshold);
    char* cls_to_name(int cls_id) const;

private:
    NPUScheduler npu_;
};

#endif /* _RK_DETECTOR_H_ */
