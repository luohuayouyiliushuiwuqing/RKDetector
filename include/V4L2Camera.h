#ifndef _V4L2_CAMERA_H_
#define _V4L2_CAMERA_H_

#include <linux/videodev2.h>
#include <opencv2/core/mat.hpp>
#include <vector>

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

    // Grab one frame into a cv::Mat (BGR). Blocks until frame available.
    bool read(cv::Mat& frame);

private:
    struct Buffer
    {
        void*  start;
        size_t length;
    };

    bool init_mmap();
    void cleanup();

    int                  fd_{-1};
    int                  width_{0};
    int                  height_{0};
    std::vector<Buffer>  buffers_;
};

#endif /* _V4L2_CAMERA_H_ */
