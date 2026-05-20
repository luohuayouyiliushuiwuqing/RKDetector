#ifndef _NPU_SCHEDULER_H_
#define _NPU_SCHEDULER_H_

#include "RkType.h"

class NPUScheduler
{
public:
    int  init(const char* model_path);
    void release();
    int  infer(rknn_input*  inputs,
               int          n_input,
               rknn_output* outputs,
               int          n_output);
    void releaseOutputs(rknn_output* outputs, int n_output);

    int               model_width() const;
    int               model_height() const;
    int               model_channel() const;
    int               obj_class_num() const;
    bool              is_quant() const;
    rknn_tensor_attr* input_attrs() const;
    rknn_tensor_attr* output_attrs() const;
    int               n_input() const;
    int               n_output() const;

private:
    rknn_app_context_t ctx_{};
};

#endif /* _NPU_SCHEDULER_H_ */
