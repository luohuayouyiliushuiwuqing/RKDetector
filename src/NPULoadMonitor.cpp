#include "rkdetector/NPULoadMonitor.h"
#include "rkdetector/log.h"

#include <chrono>
#include <thread>

NPULoadMonitor::NPULoadMonitor(const std::string& path)
{
    m_file.open(path);
    if (!m_file.is_open())
    {
        LOG_DEBUG("Failed to open %s. Try sudo or check debugfs.",
                  path.c_str());
        m_running = false;
    }
    m_running = true;
}

void NPULoadMonitor::start(int interval_ms)
{
    while (m_running)
    {
        if (read_load())
        {
            std::lock_guard<std::mutex> lock(mtx_);
        }
        std::this_thread::sleep_for(
            std::chrono::microseconds(interval_ms * 1000));
    }
    m_file.close();
}

void NPULoadMonitor::stop()
{
    m_running = false;
}

int NPULoadMonitor::get_core_load(int core)
{
    if (core < 0 || core > 2)
    {
        return 0;
    }
    std::lock_guard<std::mutex> lock(mtx_);

    return m_load_map[core];
}

bool NPULoadMonitor::read_load()
{
    if (!m_file.is_open())
    {
        return false;
    }

    m_file.clear();
    m_file.seekg(0, std::ios::beg);
    std::string line;
    if (!std::getline(m_file, line))
    {
        return false;
    }

    // Extract all numbers from the line
    int  vals[3] = {0, 0, 0};
    int  count   = 0;
    bool in_num  = false;
    int  num     = 0;

    for (size_t i = 0; i <= line.size() && count < 3; i++)
    {
        char c = (i < line.size()) ? line[i] : ' ';
        if (c >= '0' && c <= '9')
        {
            in_num = true;
            num    = num * 10 + (c - '0');
        }
        else
        {
            if (in_num)
            {
                vals[count++] = num;
                num           = 0;
                in_num        = false;
            }
        }
    }

    if (count == 0)
    {
        return false;
    }

    for (int i = 0; i < count && i < 3; i++)
    {
        m_load_map[i] = vals[i];
    }
    return true;
}
