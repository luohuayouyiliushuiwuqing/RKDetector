#include "rkdetector/RKTypeConversion.h"

using namespace rkdet;

std::string RKTypeConversion::RKMaskToString(rknn_core_mask core_mask)
{
    switch (core_mask)
    {
    case RKNN_NPU_CORE_AUTO:
        return "Core AUTO";
    case RKNN_NPU_CORE_0:
        return "Core 0";
    case RKNN_NPU_CORE_1:
        return "Core 1";
    case RKNN_NPU_CORE_2:
        return "Core 2";
    case RKNN_NPU_CORE_0_1:
        return "Core 0_1";
    case RKNN_NPU_CORE_0_1_2:
        return "Core 0_1_2";
    case RKNN_NPU_CORE_ALL:
        return "Core ALL";
    case RKNN_NPU_CORE_UNDEFINED:
        return "UNCore DEFINED";
    default:
        return "UNKNOWN";
    }
}
BoxLRTB_T RKTypeConversion::RKBoxXYWHToLRTB(const BoxXYWH_T& box_xywh)
{
    BoxLRTB_T box_lrtb;
    box_lrtb.left   = box_xywh.center_x - box_xywh.width / 2;
    box_lrtb.right  = box_xywh.center_x + box_xywh.width / 2;
    box_lrtb.top    = box_xywh.center_y - box_xywh.height / 2;
    box_lrtb.bottom = box_xywh.center_y + box_xywh.height / 2;
    return box_lrtb;
}

BoxXYWH_T RKTypeConversion::RKBoxLRTBToXYWH(const BoxLRTB_T& box_lrtb)
{
    BoxXYWH_T box_xywh;
    box_xywh.center_x = (box_lrtb.left + box_lrtb.right) / 2;
    box_xywh.center_y = (box_lrtb.top + box_lrtb.bottom) / 2;
    box_xywh.width    = box_lrtb.right - box_lrtb.left;
    box_xywh.height   = box_lrtb.bottom - box_lrtb.top;
    return box_xywh;
}
BoxLRTB_T RKTypeConversion::ObjResultToBoxLRTB(const object_detect_result& obj)
{

}
