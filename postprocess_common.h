#ifndef _RKNN_POSTPROCESS_COMMON_H_
#define _RKNN_POSTPROCESS_COMMON_H_

#include "RkType.h"

#include <vector>

void   dump_tensor_attr(rknn_tensor_attr* attr);

int    init_post_process(const char* locationFilename);
void   deinit_post_process();
char*  coco_cls_to_name(int cls_id);

// Shared utilities used by model-specific postprocess
int    clamp(float val, int min, int max);
int8_t qnt_f32_to_affine(float f32, int32_t zp, float scale);
float  deqnt_affine_to_f32(int8_t qnt, int32_t zp, float scale);
void   compute_dfl(float* tensor, int dfl_len, float* box);
int    quick_sort_indice_inverse(std::vector<float>& input,
                                 int                 left,
                                 int                 right,
                                 std::vector<int>&   indices);
int    nms(int                 validCount,
           std::vector<float>& outputLocations,
           std::vector<int>    classIds,
           std::vector<int>&   order,
           int                 filterId,
           float               threshold);

#endif //_RKNN_POSTPROCESS_COMMON_H_
