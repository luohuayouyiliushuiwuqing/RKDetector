#ifndef RKDETECTOR_RKDCOMMON_H
#define RKDETECTOR_RKDCOMMON_H
#include <string>

namespace rkdet
{

template <typename T>
struct BoxLRTB
{
    T left, right, top, bottom;
};

using BoxLRTB_T   = BoxLRTB<int>;
using BoxLRTB_I_T = BoxLRTB<int>;

template <typename T>

struct BoxXYWH
{
    T center_x, center_y, width, height;
};

using BoxXYWH_T   = BoxXYWH<int>;
using BoxXYWH_I_T = BoxXYWH<int>;

struct ObjectBox
{
    uint64_t    timestamp = 0;
    float       prop      = 0.0;
    int         type_id   = -1;
    std::string type_name = "";
    BoxLRTB_T   box_lrtb;
    BoxXYWH_T   box_xywh;
};

} // namespace rkdet

#endif //RKDETECTOR_RKDCOMMON_H
