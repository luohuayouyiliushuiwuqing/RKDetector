#pragma once
#ifndef RKDETECTOR_RKTYPECONVERSION_H
#define RKDETECTOR_RKTYPECONVERSION_H
#include "RKDCommon.h"
#include "RkType.h"
#include "rknn_api.h"

#include <string>

namespace rkdet
{

class RKTypeConversion
{
public:
    static std::string RKMaskToString(rknn_core_mask core_mask);

    static BoxLRTB_T   RKBoxXYWHToLRTB(const BoxXYWH_T& box_xywh);
    static BoxXYWH_T   RKBoxLRTBToXYWH(const BoxLRTB_T& box_lrtb);

    static BoxLRTB_T ObjResultToBoxLRTB(const object_detect_result& obj);
    static BoxXYWH_T ObjResultToBoxXYWH(const object_detect_result& obj);

};

} // namespace rkdet

#endif //RKDETECTOR_RKTYPECONVERSION_H
