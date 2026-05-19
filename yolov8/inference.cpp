#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "inference.h"
#include "common.h"
#include "image_utils.h"

int inference_model(rknn_app_context_t*        app_ctx,
                    image_buffer_t*            img,
                    object_detect_result_list* od_results)
{
    int            ret;
    image_buffer_t dst_img;
    letterbox_t    letter_box;
    rknn_input     inputs[app_ctx->io_num.n_input];
    rknn_output    outputs[app_ctx->io_num.n_output];
    const float    nms_threshold      = NMS_THRESH;
    const float    box_conf_threshold = BOX_THRESH;
    int            bg_color           = 114;

    if ((!app_ctx) || !(img) || (!od_results))
    {
        return -1;
    }

    memset(od_results, 0x00, sizeof(*od_results));
    memset(&letter_box, 0, sizeof(letterbox_t));
    memset(&dst_img, 0, sizeof(image_buffer_t));
    memset(inputs, 0, sizeof(inputs));
    memset(outputs, 0, sizeof(outputs));

    letter_box.scale  = 1;
    letter_box.x_pad  = 0;
    letter_box.y_pad  = 0;

    // Pre Process
    dst_img.width     = app_ctx->model_width;
    dst_img.height    = app_ctx->model_height;
    dst_img.format    = IMAGE_FORMAT_RGB888;
    dst_img.size      = get_image_size(&dst_img);
    dst_img.virt_addr = nullptr;

    printf("img.height=%d, img.width=%d, img.format=%d\n",
           img->height,
           img->width,
           img->format);
    printf("dst_img.height=%d, dst_img.width=%d, dst_img.format=%d\n",
           dst_img.height,
           dst_img.width,
           dst_img.format);

    if (img->height != dst_img.height || img->width != dst_img.width)
    {
        // letterbox
        dst_img.virt_addr = static_cast<unsigned char*>(malloc(dst_img.size));
        ret =
            convert_image_with_letterbox(img, &dst_img, &letter_box, bg_color);
        if (ret < 0)
        {
            printf("convert_image_with_letterbox fail! ret=%d\n", ret);
            return -1;
        }
    }
    else
    {
        dst_img.virt_addr = img->virt_addr;
    }

    // Set Input Data
    inputs[0].index = 0;
    inputs[0].type  = RKNN_TENSOR_UINT8;
    inputs[0].fmt   = RKNN_TENSOR_NHWC;
    inputs[0].size =
        app_ctx->model_width * app_ctx->model_height * app_ctx->model_channel;
    inputs[0].buf = dst_img.virt_addr;

    ret = rknn_inputs_set(app_ctx->rknn_ctx, app_ctx->io_num.n_input, inputs);
    if (ret < 0)
    {
        printf("rknn_input_set fail! ret=%d\n", ret);
        return -1;
    }

    // Run
    printf("rknn_run\n");
    ret = rknn_run(app_ctx->rknn_ctx, nullptr);
    if (ret < 0)
    {
        printf("rknn_run fail! ret=%d\n", ret);
        return -1;
    }

    // Get Output
    memset(outputs, 0, sizeof(outputs));
    for (int i = 0; i < app_ctx->io_num.n_output; i++)
    {
        outputs[i].index      = i;
        outputs[i].want_float = (!app_ctx->is_quant);
    }
    ret = rknn_outputs_get(
        app_ctx->rknn_ctx, app_ctx->io_num.n_output, outputs, NULL);
    if (ret < 0)
    {
        printf("rknn_outputs_get fail! ret=%d\n", ret);
        return ret;
    }

    // Post Process
    post_process(app_ctx,
                 outputs,
                 &letter_box,
                 box_conf_threshold,
                 nms_threshold,
                 od_results);

    rknn_outputs_release(app_ctx->rknn_ctx, app_ctx->io_num.n_output, outputs);

    return 0;
}
