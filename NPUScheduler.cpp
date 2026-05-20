#include "NPUScheduler.h"
#include "postprocess_common.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int NPUScheduler::init(const char* model_path)
{
    int          ret;
    rknn_context ctx = 0;

    ret = rknn_init(&ctx, (char*)model_path, 0, 0, NULL);
    if (ret < 0)
    {
        LOG_ERROR("rknn_init fail! ret=%d", ret);
        return -1;
    }

    rknn_sdk_version version;
    ret = rknn_query(
        ctx, RKNN_QUERY_SDK_VERSION, &version, sizeof(rknn_sdk_version));
    if (ret != RKNN_SUCC)
    {
        LOG_ERROR("rknn query version failed");
        return -1;
    }
    LOG_INFO("sdk api version: %s driver version: %s",
             version.api_version,
             version.drv_version);

    rknn_input_output_num io_num;
    ret = rknn_query(ctx, RKNN_QUERY_IN_OUT_NUM, &io_num, sizeof(io_num));
    if (ret != RKNN_SUCC)
    {
        LOG_ERROR("rknn_query IN_OUT_NUM fail! ret=%d", ret);
        return -1;
    }
    LOG_DEBUG("model input num: %d, output num: %d",
              io_num.n_input,
              io_num.n_output);

    LOG_DEBUG("input tensors:");
    rknn_tensor_attr input_attrs[io_num.n_input];
    memset(input_attrs, 0, sizeof(input_attrs));
    for (int i = 0; i < io_num.n_input; i++)
    {
        input_attrs[i].index = i;
        ret                  = rknn_query(ctx,
                         RKNN_QUERY_INPUT_ATTR,
                         &(input_attrs[i]),
                         sizeof(rknn_tensor_attr));
        if (ret != RKNN_SUCC)
        {
            LOG_ERROR("rknn_query INPUT_ATTR fail! ret=%d", ret);
            return -1;
        }
        dump_tensor_attr(&(input_attrs[i]));
    }

    LOG_DEBUG("output tensors:");
    rknn_tensor_attr output_attrs[io_num.n_output];
    memset(output_attrs, 0, sizeof(output_attrs));
    for (int i = 0; i < io_num.n_output; i++)
    {
        output_attrs[i].index = i;
        ret                   = rknn_query(ctx,
                         RKNN_QUERY_OUTPUT_ATTR,
                         &(output_attrs[i]),
                         sizeof(rknn_tensor_attr));
        if (ret != RKNN_SUCC)
        {
            LOG_ERROR("rknn_query OUTPUT_ATTR fail! ret=%d", ret);
            return -1;
        }
        dump_tensor_attr(&(output_attrs[i]));
    }

    ctx_.rknn_ctx = ctx;

    if (output_attrs[0].qnt_type == RKNN_TENSOR_QNT_AFFINE_ASYMMETRIC &&
        output_attrs[0].type == RKNN_TENSOR_INT8)
    {
        ctx_.is_quant = true;
    }
    else
    {
        ctx_.is_quant = false;
    }

    ctx_.io_num = io_num;
    ctx_.input_attrs =
        (rknn_tensor_attr*)malloc(io_num.n_input * sizeof(rknn_tensor_attr));
    memcpy(ctx_.input_attrs,
           input_attrs,
           io_num.n_input * sizeof(rknn_tensor_attr));
    ctx_.output_attrs =
        (rknn_tensor_attr*)malloc(io_num.n_output * sizeof(rknn_tensor_attr));
    memcpy(ctx_.output_attrs,
           output_attrs,
           io_num.n_output * sizeof(rknn_tensor_attr));

    ctx_.obj_class_num = output_attrs[1].dims[1];
    LOG_DEBUG("model obj_class_num=%d", ctx_.obj_class_num);

    if (input_attrs[0].fmt == RKNN_TENSOR_NCHW)
    {
        LOG_DEBUG("model is NCHW input fmt");
        ctx_.model_channel = input_attrs[0].dims[1];
        ctx_.model_height  = input_attrs[0].dims[2];
        ctx_.model_width   = input_attrs[0].dims[3];
    }
    else
    {
        LOG_DEBUG("model is NHWC input fmt");
        ctx_.model_height  = input_attrs[0].dims[1];
        ctx_.model_width   = input_attrs[0].dims[2];
        ctx_.model_channel = input_attrs[0].dims[3];
    }
    LOG_DEBUG("model input height=%d, width=%d, channel=%d",
              ctx_.model_height,
              ctx_.model_width,
              ctx_.model_channel);

    return 0;
}

void NPUScheduler::release()
{
    if (ctx_.input_attrs != NULL)
    {
        free(ctx_.input_attrs);
        ctx_.input_attrs = NULL;
    }
    if (ctx_.output_attrs != NULL)
    {
        free(ctx_.output_attrs);
        ctx_.output_attrs = NULL;
    }
    if (ctx_.rknn_ctx != 0)
    {
        rknn_destroy(ctx_.rknn_ctx);
        ctx_.rknn_ctx = 0;
    }
}

int NPUScheduler::infer(rknn_input*  inputs,
                        int          n_input,
                        rknn_output* outputs,
                        int          n_output)
{
    int ret = rknn_inputs_set(ctx_.rknn_ctx, n_input, inputs);
    if (ret < 0)
    {
        LOG_ERROR("rknn_inputs_set fail! ret=%d", ret);
        return -1;
    }

    LOG_DEBUG("rknn_run");
    ret = rknn_run(ctx_.rknn_ctx, nullptr);
    if (ret < 0)
    {
        LOG_ERROR("rknn_run fail! ret=%d", ret);
        return -1;
    }

    ret = rknn_outputs_get(ctx_.rknn_ctx, n_output, outputs, NULL);
    if (ret < 0)
    {
        LOG_ERROR("rknn_outputs_get fail! ret=%d", ret);
        return -1;
    }

    return 0;
}

void NPUScheduler::releaseOutputs(rknn_output* outputs, int n_output)
{
    rknn_outputs_release(ctx_.rknn_ctx, n_output, outputs);
}

int NPUScheduler::model_width() const
{
    return ctx_.model_width;
}

int NPUScheduler::model_height() const
{
    return ctx_.model_height;
}

int NPUScheduler::model_channel() const
{
    return ctx_.model_channel;
}

int NPUScheduler::obj_class_num() const
{
    return ctx_.obj_class_num;
}

bool NPUScheduler::is_quant() const
{
    return ctx_.is_quant;
}

rknn_tensor_attr* NPUScheduler::input_attrs() const
{
    return ctx_.input_attrs;
}

rknn_tensor_attr* NPUScheduler::output_attrs() const
{
    return ctx_.output_attrs;
}

int NPUScheduler::n_input() const
{
    return ctx_.io_num.n_input;
}

int NPUScheduler::n_output() const
{
    return ctx_.io_num.n_output;
}
