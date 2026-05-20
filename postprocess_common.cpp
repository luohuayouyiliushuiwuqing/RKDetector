#include "postprocess_common.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LABEL_NALE_TXT_PATH "./model/drone.txt"

static char* labels[OBJ_NUMB_MAX_SIZE];
static int   loaded_label_count = 0;

static char* readLine(FILE* fp, char* buffer, int* len)
{
    int    ch;
    int    i        = 0;
    size_t buff_len = 0;

    buffer          = (char*)malloc(buff_len + 1);
    if (!buffer)
    {
        return NULL;
    }

    while ((ch = fgetc(fp)) != '\n' && ch != EOF)
    {
        buff_len++;
        void* tmp = realloc(buffer, buff_len + 1);
        if (tmp == NULL)
        {
            free(buffer);
            return NULL;
        }
        buffer    = (char*)tmp;

        buffer[i] = (char)ch;
        i++;
    }
    buffer[i] = '\0';

    *len      = buff_len;

    if (ch == EOF && (i == 0 || ferror(fp)))
    {
        free(buffer);
        return NULL;
    }
    return buffer;
}

static int readLines(const char* fileName, char* lines[], int max_line)
{
    FILE* file = fopen(fileName, "r");
    char* s;
    int   i = 0;
    int   n = 0;

    if (file == NULL)
    {
        printf("Open %s fail!\n", fileName);
        return -1;
    }

    while ((s = readLine(file, s, &n)) != NULL)
    {
        lines[i++] = s;
        if (i >= max_line)
        {
            break;
        }
    }
    fclose(file);
    return i;
}

static int loadLabelName(const char* locationFilename, char* label[])
{
    printf("load lable %s\n", locationFilename);
    loaded_label_count = readLines(locationFilename, label, OBJ_NUMB_MAX_SIZE);
    return 0;
}

int clamp(float val, int min, int max)
{
    return val > min ? (val < max ? val : max) : min;
}

float CalculateOverlap(float xmin0,
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

int nms(int                 validCount,
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

int quick_sort_indice_inverse(std::vector<float>& input,
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

static int32_t __clip(float val, float min, float max)
{
    float f = val <= min ? min : (val >= max ? max : val);
    return f;
}

int8_t qnt_f32_to_affine(float f32, int32_t zp, float scale)
{
    float  dst_val = (f32 / scale) + zp;
    int8_t res     = (int8_t)__clip(dst_val, -128, 127);
    return res;
}

float deqnt_affine_to_f32(int8_t qnt, int32_t zp, float scale)
{
    return ((float)qnt - (float)zp) * scale;
}

void compute_dfl(float* tensor, int dfl_len, float* box)
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

int init_post_process()
{
    int ret = 0;
    ret     = loadLabelName(LABEL_NALE_TXT_PATH, labels);
    if (ret < 0)
    {
        printf("Load %s failed!\n", LABEL_NALE_TXT_PATH);
        return -1;
    }
    return 0;
}

char* coco_cls_to_name(int cls_id)
{
    if (cls_id >= loaded_label_count)
    {
        return "null";
    }

    if (labels[cls_id])
    {
        return labels[cls_id];
    }

    return "null";
}

void deinit_post_process()
{
    for (int i = 0; i < loaded_label_count; i++)
    {
        if (labels[i] != nullptr)
        {
            free(labels[i]);
            labels[i] = nullptr;
        }
    }
}
