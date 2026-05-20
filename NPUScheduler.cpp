#include "NPUScheduler.h"
#include "log.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <set>
#include <vector>

// ---------------------------------------------------------------------------
// Utility helpers (from postprocess_common)
// ---------------------------------------------------------------------------

static void dump_tensor_attr(rknn_tensor_attr* attr)
{
    LOG_DEBUG("  index=%d, name=%s, n_dims=%d, dims=[%d, %d, %d, %d], "
              "n_elems=%d, size=%d, fmt=%s, type=%s, qnt_type=%s, "
              "zp=%d, scale=%f",
              attr->index,
              attr->name,
              attr->n_dims,
              attr->dims[0],
              attr->dims[1],
              attr->dims[2],
              attr->dims[3],
              attr->n_elems,
              attr->size,
              get_format_string(attr->fmt),
              get_type_string(attr->type),
              get_qnt_type_string(attr->qnt_type),
              attr->zp,
              attr->scale);
}

static int clamp(float val, int min, int max)
{
    return val > min ? (val < max ? val : max) : min;
}

static int8_t qnt_f32_to_affine(float f32, int32_t zp, float scale)
{
    float dst_val = (f32 / scale) + zp;
    float f = dst_val <= -128.f ? -128.f : (dst_val >= 127.f ? 127.f : dst_val);
    return (int8_t)f;
}

static float deqnt_affine_to_f32(int8_t qnt, int32_t zp, float scale)
{
    return ((float)qnt - (float)zp) * scale;
}

static void compute_dfl(float* tensor, int dfl_len, float* box)
{
    for (int b = 0; b < 4; b++)
    {
        float exp_t[dfl_len];
        float exp_sum = 0;
        float acc_sum = 0;
        for (int i = 0; i < dfl_len; i++)
        {
            exp_t[i]  = exp(tensor[i + b * dfl_len]);
            exp_sum  += exp_t[i];
        }
        for (int i = 0; i < dfl_len; i++)
        {
            acc_sum += exp_t[i] / exp_sum * i;
        }
        box[b] = acc_sum;
    }
}

static float CalculateOverlap(float xmin0,
                              float ymin0,
                              float xmax0,
                              float ymax0,
                              float xmin1,
                              float ymin1,
                              float xmax1,
                              float ymax1)
{
    float w = fmax(0.f, fmin(xmax0, xmax1) - fmax(xmin0, xmin1) + 1.0);
    float h = fmax(0.f, fmin(ymax0, ymax1) - fmax(ymin0, ymin1) + 1.0);
    float i = w * h;
    float u = (xmax0 - xmin0 + 1.0) * (ymax0 - ymin0 + 1.0) +
              (xmax1 - xmin1 + 1.0) * (ymax1 - ymin1 + 1.0) - i;
    return u <= 0.f ? 0.f : (i / u);
}

static int nms(int                 validCount,
               std::vector<float>& outputLocations,
               std::vector<int>    classIds,
               std::vector<int>&   order,
               int                 filterId,
               float               threshold)
{
    for (int i = 0; i < validCount; ++i)
    {
        int n = order[i];
        if (n == -1 || classIds[n] != filterId)
        {
            continue;
        }
        for (int j = i + 1; j < validCount; ++j)
        {
            int m = order[j];
            if (m == -1 || classIds[m] != filterId)
            {
                continue;
            }
            float xmin0 = outputLocations[n * 4 + 0];
            float ymin0 = outputLocations[n * 4 + 1];
            float xmax0 =
                outputLocations[n * 4 + 0] + outputLocations[n * 4 + 2];
            float ymax0 =
                outputLocations[n * 4 + 1] + outputLocations[n * 4 + 3];

            float xmin1 = outputLocations[m * 4 + 0];
            float ymin1 = outputLocations[m * 4 + 1];
            float xmax1 =
                outputLocations[m * 4 + 0] + outputLocations[m * 4 + 2];
            float ymax1 =
                outputLocations[m * 4 + 1] + outputLocations[m * 4 + 3];

            float iou = CalculateOverlap(
                xmin0, ymin0, xmax0, ymax0, xmin1, ymin1, xmax1, ymax1);

            if (iou > threshold)
            {
                order[j] = -1;
            }
        }
    }
    return 0;
}

static int quick_sort_indice_inverse(std::vector<float>& input,
                                     int                 left,
                                     int                 right,
                                     std::vector<int>&   indices)
{
    float key;
    int   key_index;
    int   low  = left;
    int   high = right;
    if (left < right)
    {
        key_index = indices[left];
        key       = input[left];
        while (low < high)
        {
            while (low < high && input[high] <= key)
            {
                high--;
            }
            input[low]   = input[high];
            indices[low] = indices[high];
            while (low < high && input[low] >= key)
            {
                low++;
            }
            input[high]   = input[low];
            indices[high] = indices[low];
        }
        input[low]   = key;
        indices[low] = key_index;
        quick_sort_indice_inverse(input, left, low - 1, indices);
        quick_sort_indice_inverse(input, low + 1, right, indices);
    }
    return low;
}

// ---------------------------------------------------------------------------
// NPUScheduler — init / release / infer
// ---------------------------------------------------------------------------

int NPUScheduler::init(const char* model_path, rknn_core_mask core_mask)
{
    int          ret;
    rknn_context ctx = 0;

    ret              = rknn_init(&ctx, (char*)model_path, 0, 0, NULL);
    if (ret < 0)
    {
        LOG_ERROR("rknn_init fail! ret=%d", ret);
        return -1;
    }

    ret = rknn_set_core_mask(ctx, core_mask);
    if (ret != RKNN_SUCC)
    {
        LOG_ERROR("rknn_set_core_mask fail! ret=%d", ret);
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
    LOG_DEBUG(
        "model input num: %d, output num: %d", io_num.n_input, io_num.n_output);

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

    // // Detect model type: v8 has 6 outputs (3 branches × 2), v26 has 3
    // if (io_num.n_output == 6)
    // {
    //     type_              = ModelType::V8;
    //     // ctx_.obj_class_num = output_attrs[1].dims[1];
    //     LOG_INFO("model type: YOLOv8 (6 outputs)");
    // }
    // else
    // {
    //     type_              = ModelType::V26;
    //     // ctx_.obj_class_num = output_attrs[0].dims[1] - 4;
    //     LOG_INFO("model type: YOLOv26 (3 outputs)");
    // }

    // TODO  Currently, only YOLOv8 is supported.
    type_              = ModelType::V8;
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

int NPUScheduler::infer(rknn_input* inputs, rknn_output* outputs)
{
    auto t0  = getTimeStamp();
    int  ret = rknn_inputs_set(ctx_.rknn_ctx, ctx_.io_num.n_input, inputs);
    if (ret < 0)
    {
        LOG_ERROR("rknn_inputs_set fail! ret=%d", ret);
        return -1;
    }
    auto t1 = getTimeStamp();

    LOG_DEBUG("rknn_run");
    ret = rknn_run(ctx_.rknn_ctx, nullptr);
    if (ret < 0)
    {
        LOG_ERROR("rknn_run fail! ret=%d", ret);
        return -1;
    }

    auto t2 = getTimeStamp();

    ret =
        rknn_outputs_get(ctx_.rknn_ctx, ctx_.io_num.n_output, outputs, nullptr);
    if (ret < 0)
    {
        LOG_ERROR("rknn_outputs_get fail! ret=%d", ret);
        return -1;
    }

    auto t3 = getTimeStamp();

    LOG_DEBUG("rknn_inputs_set: %.2f ms, rknn_run: %.2f ms, rknn_outputs_get: "
              "%.2f ms",
              (t1 - t0) / 1000.0,
              (t2 - t1) / 1000.0,
              (t3 - t2) / 1000.0);

    return 0;
}

void NPUScheduler::releaseOutputs(rknn_output* outputs, int n_output)
{
    rknn_outputs_release(ctx_.rknn_ctx, n_output, outputs);
}

// ---------------------------------------------------------------------------
// YOLOv8 postprocess helpers
// ---------------------------------------------------------------------------

int NPUScheduler::v8_process_i8(rknn_output*        outputs,
                                std::vector<float>& boxes,
                                std::vector<float>& objProbs,
                                std::vector<int>&   classId,
                                float               threshold)
{
    int validCount        = 0;
    int output_per_branch = ctx_.io_num.n_output / 3;
    int dfl_len           = ctx_.output_attrs[0].dims[1] / 4;
    int model_in_h        = ctx_.model_height;

    for (int i = 0; i < 3; i++)
    {
        void*   score_sum       = nullptr;
        int32_t score_sum_zp    = 0;
        float   score_sum_scale = 1.0;
        if (output_per_branch == 3)
        {
            score_sum    = outputs[i * output_per_branch + 2].buf;
            score_sum_zp = ctx_.output_attrs[i * output_per_branch + 2].zp;
            score_sum_scale =
                ctx_.output_attrs[i * output_per_branch + 2].scale;
        }
        int     box_idx      = i * output_per_branch;
        int     score_idx    = i * output_per_branch + 1;

        int     grid_h       = ctx_.output_attrs[box_idx].dims[2];
        int     grid_w       = ctx_.output_attrs[box_idx].dims[3];
        int     stride       = model_in_h / grid_h;
        int     grid_len     = grid_h * grid_w;

        int8_t* box_tensor   = (int8_t*)outputs[box_idx].buf;
        int32_t box_zp       = ctx_.output_attrs[box_idx].zp;
        float   box_scale    = ctx_.output_attrs[box_idx].scale;
        int8_t* score_tensor = (int8_t*)outputs[score_idx].buf;
        int32_t score_zp     = ctx_.output_attrs[score_idx].zp;
        float   score_scale  = ctx_.output_attrs[score_idx].scale;

        int8_t  score_thres_i8 =
            qnt_f32_to_affine(threshold, score_zp, score_scale);
        int8_t score_sum_thres_i8 =
            qnt_f32_to_affine(threshold, score_sum_zp, score_sum_scale);

        for (int r = 0; r < grid_h; r++)
        {
            for (int c = 0; c < grid_w; c++)
            {
                int offset       = r * grid_w + c;
                int max_class_id = -1;

                if (score_sum != nullptr)
                {
                    if (((int8_t*)score_sum)[offset] < score_sum_thres_i8)
                    {
                        continue;
                    }
                }

                int8_t max_score = -score_zp;
                for (int k = 0; k < ctx_.obj_class_num; k++)
                {
                    if ((score_tensor[offset] > score_thres_i8) &&
                        (score_tensor[offset] > max_score))
                    {
                        max_score    = score_tensor[offset];
                        max_class_id = k;
                    }
                    offset += grid_len;
                }

                if (max_score > score_thres_i8)
                {
                    offset = r * grid_w + c;
                    float box[4];
                    float before_dfl[dfl_len * 4];
                    for (int k = 0; k < dfl_len * 4; k++)
                    {
                        before_dfl[k] = deqnt_affine_to_f32(
                            box_tensor[offset], box_zp, box_scale);
                        offset += grid_len;
                    }
                    compute_dfl(before_dfl, dfl_len, box);

                    float x1 = (-box[0] + c + 0.5f) * stride;
                    float y1 = (-box[1] + r + 0.5f) * stride;
                    float x2 = (box[2] + c + 0.5f) * stride;
                    float y2 = (box[3] + r + 0.5f) * stride;
                    boxes.push_back(x1);
                    boxes.push_back(y1);
                    boxes.push_back(x2 - x1);
                    boxes.push_back(y2 - y1);

                    objProbs.push_back(
                        deqnt_affine_to_f32(max_score, score_zp, score_scale));
                    classId.push_back(max_class_id);
                    validCount++;
                }
            }
        }
    }
    return validCount;
}

int NPUScheduler::v8_process_fp32(rknn_output*        outputs,
                                  std::vector<float>& boxes,
                                  std::vector<float>& objProbs,
                                  std::vector<int>&   classId,
                                  float               threshold)
{
    int validCount        = 0;
    int output_per_branch = ctx_.io_num.n_output / 3;
    int dfl_len           = ctx_.output_attrs[0].dims[1] / 4;
    int model_in_h        = ctx_.model_height;

    for (int i = 0; i < 3; i++)
    {
        float* score_sum = nullptr;
        if (output_per_branch == 3)
        {
            score_sum = (float*)outputs[i * output_per_branch + 2].buf;
        }
        int    box_idx      = i * output_per_branch;
        int    score_idx    = i * output_per_branch + 1;

        int    grid_h       = ctx_.output_attrs[box_idx].dims[2];
        int    grid_w       = ctx_.output_attrs[box_idx].dims[3];
        int    stride       = model_in_h / grid_h;
        int    grid_len     = grid_h * grid_w;

        float* box_tensor   = (float*)outputs[box_idx].buf;
        float* score_tensor = (float*)outputs[score_idx].buf;

        for (int r = 0; r < grid_h; r++)
        {
            for (int c = 0; c < grid_w; c++)
            {
                int offset       = r * grid_w + c;
                int max_class_id = -1;

                if (score_sum != nullptr)
                {
                    if (score_sum[offset] < threshold)
                    {
                        continue;
                    }
                }

                float max_score = 0;
                for (int k = 0; k < ctx_.obj_class_num; k++)
                {
                    if ((score_tensor[offset] > threshold) &&
                        (score_tensor[offset] > max_score))
                    {
                        max_score    = score_tensor[offset];
                        max_class_id = k;
                    }
                    offset += grid_len;
                }

                if (max_score > threshold)
                {
                    offset = r * grid_w + c;
                    float box[4];
                    float before_dfl[dfl_len * 4];
                    for (int k = 0; k < dfl_len * 4; k++)
                    {
                        before_dfl[k]  = box_tensor[offset];
                        offset        += grid_len;
                    }
                    compute_dfl(before_dfl, dfl_len, box);

                    float x1 = (-box[0] + c + 0.5f) * stride;
                    float y1 = (-box[1] + r + 0.5f) * stride;
                    float x2 = (box[2] + c + 0.5f) * stride;
                    float y2 = (box[3] + r + 0.5f) * stride;
                    boxes.push_back(x1);
                    boxes.push_back(y1);
                    boxes.push_back(x2 - x1);
                    boxes.push_back(y2 - y1);

                    objProbs.push_back(max_score);
                    classId.push_back(max_class_id);
                    validCount++;
                }
            }
        }
    }
    return validCount;
}

// ---------------------------------------------------------------------------
// Unified post_process
// ---------------------------------------------------------------------------

int NPUScheduler::post_process(rknn_output*               outputs,
                               const letterbox_t*         letter_box,
                               float                      conf_threshold,
                               float                      nms_threshold,
                               object_detect_result_list* od_results)
{
    std::vector<float> filterBoxes;
    std::vector<float> objProbs;
    std::vector<int>   classId;
    int                validCount = 0;
    int                model_in_w = ctx_.model_width;
    int                model_in_h = ctx_.model_height;

    memset(od_results, 0, sizeof(object_detect_result_list));

    if (type_ == ModelType::V8)
    {
        if (ctx_.is_quant)
        {
            validCount = v8_process_i8(
                outputs, filterBoxes, objProbs, classId, conf_threshold);
        }
        else
        {
            validCount = v8_process_fp32(
                outputs, filterBoxes, objProbs, classId, conf_threshold);
        }
    }

    if (validCount <= 0)
    {
        return 0;
    }

    // Sort by confidence descending
    std::vector<int> indexArray;
    for (int i = 0; i < validCount; ++i)
    {
        indexArray.push_back(i);
    }
    quick_sort_indice_inverse(objProbs, 0, validCount - 1, indexArray);

    // v8: per-class NMS; v26: no NMS (model suppresses internally)
    if (type_ == ModelType::V8)
    {
        std::set<int> class_set(std::begin(classId), std::end(classId));
        for (auto c : class_set)
        {
            nms(validCount, filterBoxes, classId, indexArray, c, nms_threshold);
        }
    }

    // Assemble results
    int last_count    = 0;
    od_results->count = 0;

    for (int i = 0; i < validCount; ++i)
    {
        if (indexArray[i] == -1 || last_count >= OBJ_NUMB_MAX_SIZE)
        {
            continue;
        }
        int   n        = indexArray[i];

        float x1       = filterBoxes[n * 4 + 0] - letter_box->x_pad;
        float y1       = filterBoxes[n * 4 + 1] - letter_box->y_pad;
        float x2       = x1 + filterBoxes[n * 4 + 2];
        float y2       = y1 + filterBoxes[n * 4 + 3];
        int   id       = classId[n];
        float obj_conf = objProbs[i];

        od_results->results[last_count].box.left =
            (int)(clamp(x1, 0, model_in_w) / letter_box->scale);
        od_results->results[last_count].box.top =
            (int)(clamp(y1, 0, model_in_h) / letter_box->scale);
        od_results->results[last_count].box.right =
            (int)(clamp(x2, 0, model_in_w) / letter_box->scale);
        od_results->results[last_count].box.bottom =
            (int)(clamp(y2, 0, model_in_h) / letter_box->scale);
        od_results->results[last_count].prop   = obj_conf;
        od_results->results[last_count].cls_id = id;
        last_count++;
    }
    od_results->count = last_count;
    return 0;
}

// ---------------------------------------------------------------------------
// Metadata accessors
// ---------------------------------------------------------------------------

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

ModelType NPUScheduler::model_type() const
{
    return type_;
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
