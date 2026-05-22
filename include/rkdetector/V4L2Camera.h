#ifndef _RKDETECTOR_V4L2_CAMERA_H_
#define _RKDETECTOR_V4L2_CAMERA_H_

#include <linux/videodev2.h>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace rkdet {

struct V4L2Frame
{
    void*    data;
    uint32_t size;
    uint32_t width;
    uint32_t height;
};

class V4L2Camera
{
public:
    V4L2Camera();
    ~V4L2Camera();

    bool open(const char* device,
              int         width  = 640,
              int         height = 480,
              int         fps    = 30);

    void close();
    bool is_opened() const;

    // Grab one NV12 frame. Blocks until frame available.
    bool read(V4L2Frame& frame);

private:
    struct Buffer
    {
        void*  start;
        size_t length;
    };

    bool init_mmap();
    void cleanup();

    int                  fd_{-1};
    uint32_t             width_{0};
    uint32_t             height_{0};
    std::vector<Buffer>  buffers_;
};

} // namespace rkdet

#endif /* _RKDETECTOR_V4L2_CAMERA_H_ */
