#include "RKDetector.h"
#include "image_utils.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>

int RKDetector::init(const char* model_path, rknn_core_mask core_mask)
{
    int ret = npu_.init(model_path, core_mask);
    if (ret != 0)
    {
        LOG_ERROR("npu init fail! ret=%d", ret);
        return -1;
    }
    return 0;
}

void RKDetector::release()
{
    npu_.release();
}

int RKDetector::detect(const image_buffer_t*      img,
                       object_detect_result_list* results,
                       float                      conf_threshold,
                       float                      nms_threshold)
{
    int            ret;
    image_buffer_t dst_img;
    letterbox_t    letter_box;
    rknn_input     inputs[npu_.n_input()];
    rknn_output    outputs[npu_.n_output()];

    if ((!img) || (!results))
    {
        return -1;
    }

    memset(results, 0x00, sizeof(*results));
    memset(&letter_box, 0, sizeof(letterbox_t));
    memset(&dst_img, 0, sizeof(image_buffer_t));
    memset(inputs, 0, sizeof(inputs));
    memset(outputs, 0, sizeof(outputs));

    letter_box.scale  = 1;
    letter_box.x_pad  = 0;
    letter_box.y_pad  = 0;

    // Pre Process
    dst_img.width     = npu_.model_width();
    dst_img.height    = npu_.model_height();
    dst_img.format    = IMAGE_FORMAT_RGB888;
    dst_img.size      = get_image_size(&dst_img);
    dst_img.virt_addr = nullptr;

    int need_free_dst = 0;
    if (img->height != dst_img.height || img->width != dst_img.width)
    {
        LOG_DEBUG("src_img.height=%d, src_img.width=%d, src_img.format=%d",
                  img->height,
                  img->width,
                  img->format);
        LOG_DEBUG("dst_img.height=%d, dst_img.width=%d, dst_img.format=%d",
                  dst_img.height,
                  dst_img.width,
                  dst_img.format);
        dst_img.virt_addr = static_cast<unsigned char*>(malloc(dst_img.size));
        need_free_dst     = 1;
        ret               = convert_image_with_letterbox(
            const_cast<image_buffer_t*>(img), &dst_img, &letter_box, 114);
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
    inputs[0].size =
        npu_.model_width() * npu_.model_height() * npu_.model_channel();
    inputs[0].buf = dst_img.virt_addr;

    // Run NPU
    memset(outputs, 0, sizeof(outputs));
    for (int i = 0; i < npu_.n_output(); i++)
    {
        outputs[i].index      = i;
        outputs[i].want_float = (!npu_.is_quant());
    }

    ret = npu_.infer(inputs, outputs);
    if (ret < 0)
    {
        LOG_ERROR("npu infer fail! ret=%d", ret);
        if (need_free_dst)
        {
            free(dst_img.virt_addr);
        }
        return -1;
    }

    npu_.post_process(
        outputs, &letter_box, conf_threshold, nms_threshold, results);

    npu_.releaseOutputs(outputs, npu_.n_output());

    if (need_free_dst)
    {
        free(dst_img.virt_addr);
    }

    return 0;
}