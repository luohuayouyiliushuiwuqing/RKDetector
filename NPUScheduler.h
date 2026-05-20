#ifndef _NPU_SCHEDULER_H_
#define _NPU_SCHEDULER_H_

#include "RkType.h"
#include "image_utils.h"

#include <vector>

enum class ModelType
{
    V5,
    V8,
    V26
};

class NPUScheduler
{
public:
    int  init(const char* model_path);
    void release();
    int
    infer(rknn_input* inputs, int n_input, rknn_output* outputs, int n_output);
    void              releaseOutputs(rknn_output* outputs, int n_output);

    int               init_post_process(const char* label_path);
    void              deinit_post_process();
    int               post_process(rknn_output*               outputs,
                                   const letterbox_t*         letter_box,
                                   float                      conf_threshold,
                                   float                      nms_threshold,
                                   object_detect_result_list* results);
    char*             cls_to_name(int cls_id) const;

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
    rknn_app_context_t ctx_{};
    ModelType          type_{ModelType::V8};

    // label
    char*              labels_[OBJ_NUMB_MAX_SIZE]{};
    int                label_count_{};

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

#endif /* _NPU_SCHEDULER_H_ */
