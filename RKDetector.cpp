#include "RKDetector.h"
#include "postprocess_common.h"
#include "image_utils.h"
#include "log.h"
#include "psotprocess.h"

#include <stdlib.h>
#include <string.h>

int RKDetector::init(const char* model_path, const char* label_path)
{
    int ret = npu_.init(model_path);
    if (ret != 0)
    {
        LOG_ERROR("npu init fail! ret=%d", ret);
        return -1;
    }

    ret = init_post_process(label_path);
    if (ret != 0)
    {
        LOG_ERROR("init_post_process fail! ret=%d", ret);
        npu_.release();
        return -1;
    }

    return 0;
}

void RKDetector::release()
{
    npu_.release();
    deinit_post_process();
}

int RKDetector::detect(const image_buffer_t*        img,
                       object_detect_result_list* results)
{
    int            ret;
    image_buffer_t dst_img;
    letterbox_t    letter_box;
    rknn_input     inputs[npu_.n_input()];
    rknn_output    outputs[npu_.n_output()];
    const float    nms_threshold      = NMS_THRESH;
    const float    box_conf_threshold = BOX_THRESH;
    int            bg_color           = 114;

    if ((!img) || (!results))
    {
        return -1;
    }

    memset(results, 0x00, sizeof(*results));
    memset(&letter_box, 0, sizeof(letterbox_t));
    memset(&dst_img, 0, sizeof(image_buffer_t));
    memset(inputs, 0, sizeof(inputs));
    memset(outputs, 0, sizeof(outputs));

    letter_box.scale = 1;
    letter_box.x_pad = 0;
    letter_box.y_pad = 0;

    // Pre Process
    dst_img.width     = npu_.model_width();
    dst_img.height    = npu_.model_height();
    dst_img.format    = IMAGE_FORMAT_RGB888;
    dst_img.size      = get_image_size(&dst_img);
    dst_img.virt_addr = nullptr;

    LOG_DEBUG("img.height=%d, img.width=%d, img.format=%d",
              img->height,
              img->width,
              img->format);
    LOG_DEBUG("dst_img.height=%d, dst_img.width=%d, dst_img.format=%d",
              dst_img.height,
              dst_img.width,
              dst_img.format);

    int need_free_dst = 0;
    if (img->height != dst_img.height || img->width != dst_img.width)
    {
        dst_img.virt_addr = static_cast<unsigned char*>(malloc(dst_img.size));
        need_free_dst     = 1;
        ret = convert_image_with_letterbox(
            (image_buffer_t*)img, &dst_img, &letter_box, bg_color);
        if (ret < 0)
        {
            LOG_ERROR("convert_image_with_letterbox fail! ret=%d", ret);
            free(dst_img.virt_addr);
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
    inputs[0].size  = npu_.model_width() * npu_.model_height() *
                     npu_.model_channel();
    inputs[0].buf = dst_img.virt_addr;

    // Run NPU
    memset(outputs, 0, sizeof(outputs));
    for (int i = 0; i < npu_.n_output(); i++)
    {
        outputs[i].index      = i;
        outputs[i].want_float = (!npu_.is_quant());
    }

    ret = npu_.infer(inputs, npu_.n_input(), outputs, npu_.n_output());
    if (ret < 0)
    {
        LOG_ERROR("npu infer fail! ret=%d", ret);
        if (need_free_dst)
            free(dst_img.virt_addr);
        return -1;
    }

    // Post Process
    // Build a temporary rknn_app_context_t for post_process
    rknn_app_context_t tmp_ctx;
    tmp_ctx.input_attrs  = npu_.input_attrs();
    tmp_ctx.output_attrs = npu_.output_attrs();
    tmp_ctx.io_num       = {(uint32_t)npu_.n_input(), (uint32_t)npu_.n_output()};
    tmp_ctx.is_quant     = npu_.is_quant();
    tmp_ctx.model_width  = npu_.model_width();
    tmp_ctx.model_height = npu_.model_height();
    tmp_ctx.obj_class_num = npu_.obj_class_num();

    post_process(&tmp_ctx, outputs, &letter_box,
                 box_conf_threshold, nms_threshold, results);

    npu_.releaseOutputs(outputs, npu_.n_output());

    if (need_free_dst)
    {
        free(dst_img.virt_addr);
    }

    return 0;
}
