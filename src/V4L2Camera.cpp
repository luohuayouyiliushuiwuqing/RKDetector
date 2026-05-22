#include "rkdetector/V4L2Camera.h"
#include "rkdetector/log.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

using namespace rkdet;

V4L2Camera::V4L2Camera() = default;

V4L2Camera::~V4L2Camera()
{
    close();
}

bool V4L2Camera::open(const char* device,
                      int         width,
                      int         height,
                      int         fps)
{
    fd_ = ::open(device, O_RDWR | O_NONBLOCK);
    if (fd_ < 0)
    {
        LOG_ERROR("V4L2: open %s fail: %s", device, strerror(errno));
        return false;
    }

    // Set format — multiplanar API for NV12
    struct v4l2_format fmt{};
    fmt.type                     = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    fmt.fmt.pix_mp.width         = width;
    fmt.fmt.pix_mp.height        = height;
    fmt.fmt.pix_mp.pixelformat   = V4L2_PIX_FMT_NV12;
    fmt.fmt.pix_mp.field         = V4L2_FIELD_NONE;
    fmt.fmt.pix_mp.num_planes    = 1;

    if (ioctl(fd_, VIDIOC_S_FMT, &fmt) < 0)
    {
        LOG_ERROR("V4L2: set format fail: %s", strerror(errno));
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    // Driver may adjust width/height
    width_  = fmt.fmt.pix_mp.width;
    height_ = fmt.fmt.pix_mp.height;
    LOG_INFO("V4L2: %s opened, %dx%d NV12", device, width_, height_);

    // Set frame rate
    struct v4l2_streamparm parm{};
    parm.type                                  = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    parm.parm.capture.timeperframe.numerator   = 1;
    parm.parm.capture.timeperframe.denominator = fps;
    ioctl(fd_, VIDIOC_S_PARM, &parm);

    return init_mmap();
}

void V4L2Camera::close()
{
    if (fd_ >= 0)
    {
        cleanup();
        ::close(fd_);
        fd_ = -1;
    }
}

bool V4L2Camera::is_opened() const
{
    return fd_ >= 0;
}

bool V4L2Camera::init_mmap()
{
    struct v4l2_requestbuffers req{};
    req.count  = 4;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(fd_, VIDIOC_REQBUFS, &req) < 0)
    {
        LOG_ERROR("V4L2: reqbufs fail: %s", strerror(errno));
        return false;
    }

    buffers_.resize(req.count);

    for (size_t i = 0; i < buffers_.size(); i++)
    {
        struct v4l2_plane  plane{};
        struct v4l2_buffer buf{};
        buf.type     = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory   = V4L2_MEMORY_MMAP;
        buf.index    = (unsigned)i;
        buf.length   = 1;
        buf.m.planes = &plane;

        if (ioctl(fd_, VIDIOC_QUERYBUF, &buf) < 0)
        {
            LOG_ERROR("V4L2: querybuf[%zu] fail: %s", i, strerror(errno));
            return false;
        }

        buffers_[i].length = plane.length;
        buffers_[i].start  = mmap(nullptr,
                                  plane.length,
                                  PROT_READ | PROT_WRITE,
                                  MAP_SHARED,
                                  fd_,
                                  plane.m.mem_offset);

        if (buffers_[i].start == MAP_FAILED)
        {
            LOG_ERROR("V4L2: mmap[%zu] fail: %s", i, strerror(errno));
            buffers_[i].start = nullptr;
            return false;
        }

        // Queue buffer
        memset(&plane, 0, sizeof(plane));
        buf.type     = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory   = V4L2_MEMORY_MMAP;
        buf.index    = (unsigned)i;
        buf.length   = 1;
        buf.m.planes = &plane;

        if (ioctl(fd_, VIDIOC_QBUF, &buf) < 0)
        {
            LOG_ERROR("V4L2: qbuf[%zu] fail: %s", i, strerror(errno));
            return false;
        }
    }

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (ioctl(fd_, VIDIOC_STREAMON, &type) < 0)
    {
        LOG_ERROR("V4L2: streamon fail: %s", strerror(errno));
        return false;
    }

    LOG_INFO("V4L2: streaming started (%zu buffers)", buffers_.size());
    return true;
}

void V4L2Camera::cleanup()
{
    if (fd_ < 0)
        return;

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    ioctl(fd_, VIDIOC_STREAMOFF, &type);

    for (auto& b : buffers_)
    {
        if (b.start && b.start != MAP_FAILED)
        {
            munmap(b.start, b.length);
        }
    }
    buffers_.clear();
}

bool V4L2Camera::read(V4L2Frame& frame)
{
    if (fd_ < 0)
        return false;

    fd_set fds;
    struct timeval tv{};

    FD_ZERO(&fds);
    FD_SET(fd_, &fds);
    tv.tv_sec  = 2;
    tv.tv_usec = 0;

    int r = select(fd_ + 1, &fds, nullptr, nullptr, &tv);
    if (r <= 0)
    {
        LOG_WARN("V4L2: select timeout");
        return false;
    }

    struct v4l2_plane  plane{};
    struct v4l2_buffer buf{};
    buf.type     = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    buf.memory   = V4L2_MEMORY_MMAP;
    buf.length   = 1;
    buf.m.planes = &plane;

    if (ioctl(fd_, VIDIOC_DQBUF, &buf) < 0)
    {
        LOG_ERROR("V4L2: dqbuf fail: %s", strerror(errno));
        return false;
    }

    frame.data   = buffers_[buf.index].start;
    frame.size   = (uint32_t)plane.bytesused;
    frame.width  = width_;
    frame.height = height_;

    // Re-queue
    memset(&plane, 0, sizeof(plane));
    buf.type     = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    buf.memory   = V4L2_MEMORY_MMAP;
    buf.length   = 1;
    buf.m.planes = &plane;
    ioctl(fd_, VIDIOC_QBUF, &buf);

    return true;
}
