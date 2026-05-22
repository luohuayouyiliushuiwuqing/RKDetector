#ifndef _RKDETECTOR_FRAME_BUFFER_H_
#define _RKDETECTOR_FRAME_BUFFER_H_

#include <mutex>

template<typename T>
class FrameBuffer
{
public:
    void push(const T& frame)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        frame_ = frame;
        ready_ = true;
    }

    bool pop(T& frame)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!ready_)
        {
            return false;
        }
        frame  = frame_;
        ready_ = false;
        return true;
    }

private:
    T               frame_;
    bool            ready_{false};
    mutable std::mutex mtx_;
};

#endif /* _RKDETECTOR_FRAME_BUFFER_H_ */
