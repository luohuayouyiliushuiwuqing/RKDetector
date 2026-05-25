#include "rkdetector/NPULoadMonitor.h"
#include "rkdetector/log.h"

#include <chrono>
#include <thread>

using namespace rkdet;

NPULoadMonitor::NPULoadMonitor(std::string path, int npu_core_len)
: npu_core_len(npu_core_len)
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
        {
            std::lock_guard<std::mutex> lock(m_mtx);
            read_load();
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
    std::lock_guard<std::mutex> lock(m_mtx);

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
    size_t pos = 0;
    while ((pos = line.find("Core", pos)) != std::string::npos)
    {
        if (line[pos + 4] >= '0' && line[pos + 4] <= '9')
        {
            int core_id = line[pos + 4] - '0';
            if (core_id >= 0 && core_id < npu_core_len)
            {
                size_t percent_pos = line.find("%", pos);
                if (percent_pos != std::string::npos)
                {
                    // Extract the number before '%'
                    size_t num_start = line.rfind(" ", percent_pos);
                    if (num_start != std::string::npos &&
                        num_start < percent_pos)
                    {
                        std::string num_str = line.substr(
                            num_start + 1, percent_pos - num_start - 1);
                        int load_value      = std::stoi(num_str);
                        m_load_map[core_id] = load_value;
                    }
                }
            }
        }
        pos += 9; // 跳过已找到的 "Core" 避免死循环
    }
    return true;
}