#ifndef _RKDETECTOR_RK_SCHEDULER_H_
#define _RKDETECTOR_RK_SCHEDULER_H_

#include "rkdetector/RkType.h"
#include "rk_image_utils.h"

#include <vector>

enum class ModelType
{
    V5,
    V8,
    V26
};

class RKScheduler
{
public:
    int               init(const char*    model_path,
                           rknn_core_mask core_mask = RKNN_NPU_CORE_AUTO);
    void              release();
    int               infer(rknn_input* inputs, rknn_output* outputs);
    void              releaseOutputs(rknn_output* outputs, int n_output);

    int               post_process(rknn_output*               outputs,
                                   const letterbox_t*         letter_box,
                                   float                      conf_threshold,
                                   float                      nms_threshold,
                                   object_detect_result_list* results);

    int               model_width() const;
    int               model_height() const;
    int               model_channel() const;
    int               obj_class_num() const;
    bool              is_quant() const;
    ModelType         model_type() const;
    rknn_tensor_attr* input_attrs() const;
    rknn_tensor_attr* output_attrs() const;
    int               n_input() const;
    int               n_output() const;

private:
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

    rknn_app_context_t ctx_{};
    ModelType          type_{ModelType::V8};

    // postprocess helpers (return validCount)
    int                v8_process_i8(rknn_output*        outputs,
                                     std::vector<float>& boxes,
                                     std::vector<float>& objProbs,
                                     std::vector<int>&   classId,
                                     float               threshold);
    int                v8_process_fp32(rknn_output*        outputs,
                                       std::vector<float>& boxes,
                                       std::vector<float>& objProbs,
                                       std::vector<int>&   classId,
                                       float               threshold);
};

#endif /* _RKDETECTOR_RK_SCHEDULER_H_ */
