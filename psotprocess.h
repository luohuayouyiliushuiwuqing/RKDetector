#ifndef RKNN_DEMO_PSOTPROCESS_H
#define RKNN_DEMO_PSOTPROCESS_H
#include "RkType.h"
#include "image_utils.h"

// Implemented by model-specific postprocess (v8 or 26)
int post_process(rknn_app_context_t*        app_ctx,
                 void*                      outputs,
                 letterbox_t*               letter_box,
                 float                      conf_threshold,
                 float                      nms_threshold,
                 object_detect_result_list* od_results);

#endif //RKNN_DEMO_PSOTPROCESS_H
