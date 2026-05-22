#ifndef RKDETECTOR_RKDCOMMON_H
#define RKDETECTOR_RKDCOMMON_H
#include <string>

namespace rkdet {

template <typename T>
struct RD_BoxLRTB
{
    T left, right, top, bottom;
};

using RD_BoxLRTB_T   = RD_BoxLRTB<int>;
using RD_BoxLRTB_I_T = RD_BoxLRTB<int>;

template <typename T>

struct RD_BoxXYWH
{
    T center_x, center_y, width, height;
};

using RD_BoxXYWH_T   = RD_BoxXYWH<int>;
using RD_BoxXYWH_I_T = RD_BoxXYWH<int>;

struct RD_ObjectBox
{
    float          prop;
    int            type_id;
    std::string    type_name;
    RD_BoxLRTB_I_T rd_box_lrtb_int;
    RD_BoxXYWH_I_T rd_box_xywh_int;
};

} // namespace rkdet

#endif //RKDETECTOR_RKDCOMMON_H
