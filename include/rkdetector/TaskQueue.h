#ifndef _RKDETECTOR_TASK_QUEUE_H_
#define _RKDETECTOR_TASK_QUEUE_H_

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>

namespace rkdet {

class TaskQueue
{
public:
    using Task = std::function<void()>;

    explicit TaskQueue(size_t capacity = 256) : m_capacity(capacity) {}

    ~TaskQueue() { stop(); }

    bool push(Task task)
    {
        {
            std::lock_guard<std::mutex> lock(m_mtx);
            if (m_stopped)
            {
                return false;
            }
            // drop oldest if full (keep up with camera, don't block)
            while (m_queue.size() >= m_capacity)
            {
                m_queue.pop();
                m_drop_count++;
            }
            m_queue.push(std::move(task));
        }
        m_cv.notify_one();
        return true;
    }

    bool pop(Task& task)
    {
        std::unique_lock<std::mutex> lock(m_mtx);
        m_cv.wait(lock, [this] {
            return !m_queue.empty() || m_stopped;
        });
        if (m_stopped && m_queue.empty())
        {
            return false;
        }
        task = std::move(m_queue.front());
        m_queue.pop();
        return true;
    }

    void stop()
    {
        {
            std::lock_guard<std::mutex> lock(m_mtx);
            m_stopped = true;
        }
        m_cv.notify_all();
    }

    size_t size() const
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        return m_queue.size();
    }

    uint64_t drop_count() const { return m_drop_count; }

private:
    std::queue<Task>        m_queue;
    size_t                  m_capacity;
    bool                    m_stopped{false};
    mutable std::mutex      m_mtx;
    std::condition_variable m_cv;
    uint64_t                m_drop_count{0};
};

} // namespace rkdet

#endif /* _RKDETECTOR_TASK_QUEUE_H_ */
