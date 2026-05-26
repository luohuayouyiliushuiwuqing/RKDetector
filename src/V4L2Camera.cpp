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

bool V4L2Camera::open(
    const char* device, int width, int height, int fps, int format)
{
    m_fd = ::open(device, O_RDWR | O_NONBLOCK);
    if (m_fd < 0)
    {
        LOG_ERROR("V4L2: open %s fail: %s", device, strerror(errno));
        return false;
    }

    // Set format — multiplanar API for NV12
    struct v4l2_format fmt{};
    fmt.type                   = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    fmt.fmt.pix_mp.width       = width;
    fmt.fmt.pix_mp.height      = height;
    fmt.fmt.pix_mp.pixelformat = format;
    fmt.fmt.pix_mp.field       = V4L2_FIELD_NONE;
    fmt.fmt.pix_mp.num_planes  = 1;

    if (ioctl(m_fd, VIDIOC_S_FMT, &fmt) < 0)
    {
        LOG_ERROR("V4L2: set format fail: %s", strerror(errno));
        ::close(m_fd);
        m_fd = -1;
        return false;
    }

    // Driver may adjust width/height
    m_width  = fmt.fmt.pix_mp.width;
    m_height = fmt.fmt.pix_mp.height;
    LOG_DEBUG("V4L2: %s opened, %dx%d NV12", device, m_width, m_height);

    // Set frame rate
    struct v4l2_streamparm parm{};
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    parm.parm.capture.timeperframe.numerator   = 1;
    parm.parm.capture.timeperframe.denominator = fps;
    ioctl(m_fd, VIDIOC_S_PARM, &parm);

    return init_mmap();
}

void V4L2Camera::close()
{
    if (m_fd >= 0)
    {
        cleanup();
        ::close(m_fd);
        m_fd = -1;
    }
}

bool V4L2Camera::is_opened() const
{
    return m_fd >= 0;
}

bool V4L2Camera::init_mmap()
{
    struct v4l2_requestbuffers req{};
    req.count  = 4;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(m_fd, VIDIOC_REQBUFS, &req) < 0)
    {
        LOG_ERROR("V4L2: reqbufs fail: %s", strerror(errno));
        return false;
    }

    m_buffers.resize(req.count);

    for (size_t i = 0; i < m_buffers.size(); i++)
    {
        struct v4l2_plane  plane{};
        struct v4l2_buffer buf{};
        buf.type     = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory   = V4L2_MEMORY_MMAP;
        buf.index    = (unsigned)i;
        buf.length   = 1;
        buf.m.planes = &plane;

        if (ioctl(m_fd, VIDIOC_QUERYBUF, &buf) < 0)
        {
            LOG_ERROR("V4L2: querybuf[%zu] fail: %s", i, strerror(errno));
            return false;
        }

        m_buffers[i].length = plane.length;
        m_buffers[i].start  = mmap(nullptr,
                                  plane.length,
                                  PROT_READ | PROT_WRITE,
                                  MAP_SHARED,
                                  m_fd,
                                  plane.m.mem_offset);

        if (m_buffers[i].start == MAP_FAILED)
        {
            LOG_ERROR("V4L2: mmap[%zu] fail: %s", i, strerror(errno));
            m_buffers[i].start = nullptr;
            return false;
        }

        // Queue buffer
        memset(&plane, 0, sizeof(plane));
        buf.type     = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory   = V4L2_MEMORY_MMAP;
        buf.index    = (unsigned)i;
        buf.length   = 1;
        buf.m.planes = &plane;

        if (ioctl(m_fd, VIDIOC_QBUF, &buf) < 0)
        {
            LOG_ERROR("V4L2: qbuf[%zu] fail: %s", i, strerror(errno));
            return false;
        }
    }

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (ioctl(m_fd, VIDIOC_STREAMON, &type) < 0)
    {
        LOG_ERROR("V4L2: streamon fail: %s", strerror(errno));
        return false;
    }

    LOG_DEBUG("V4L2: streaming started (%zu buffers)", m_buffers.size());
    return true;
}

void V4L2Camera::cleanup()
{
    if (m_fd < 0)
    {
        return;
    }

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    ioctl(m_fd, VIDIOC_STREAMOFF, &type);

    for (auto& b : m_buffers)
    {
        if (b.start && b.start != MAP_FAILED)
        {
            munmap(b.start, b.length);
        }
    }
    m_buffers.clear();
}

bool V4L2Camera::read(V4L2Frame& frame)
{
    if (m_fd < 0)
    {
        return false;
    }

    fd_set         fds;
    struct timeval tv{};

    FD_ZERO(&fds);
    FD_SET(m_fd, &fds);
    tv.tv_sec  = 2;
    tv.tv_usec = 0;

    int r      = select(m_fd + 1, &fds, nullptr, nullptr, &tv);
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

    if (ioctl(m_fd, VIDIOC_DQBUF, &buf) < 0)
    {
        LOG_ERROR("V4L2: dqbuf fail: %s", strerror(errno));
        return false;
    }

    frame.data      = m_buffers[buf.index].start;
    frame.size      = plane.bytesused;

    // Re-queue
    memset(&plane, 0, sizeof(plane));
    buf.type     = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    buf.memory   = V4L2_MEMORY_MMAP;
    buf.length   = 1;
    buf.m.planes = &plane;
    ioctl(m_fd, VIDIOC_QBUF, &buf);

    return true;
}

bool V4L2Camera::read(void*& frame_ptr, uint32_t& frame_size)
{
    if (m_fd < 0)
    {
        return false;
    }

    fd_set         fds;
    struct timeval tv{};

    FD_ZERO(&fds);
    FD_SET(m_fd, &fds);
    tv.tv_sec  = 2;
    tv.tv_usec = 0;

    int r      = select(m_fd + 1, &fds, nullptr, nullptr, &tv);
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

    if (ioctl(m_fd, VIDIOC_DQBUF, &buf) < 0)
    {
        LOG_ERROR("V4L2: dqbuf fail: %s", strerror(errno));
        return false;
    }

    frame_ptr       = m_buffers[buf.index].start;
    frame_size      = plane.bytesused;

    // Re-queue
    memset(&plane, 0, sizeof(plane));
    buf.type     = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    buf.memory   = V4L2_MEMORY_MMAP;
    buf.length   = 1;
    buf.m.planes = &plane;
    ioctl(m_fd, VIDIOC_QBUF, &buf);

    return true;
}
