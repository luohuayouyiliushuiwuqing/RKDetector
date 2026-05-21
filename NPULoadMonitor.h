#ifndef RKDETECTOR_NPULOADMONITOR_H
#define RKDETECTOR_NPULOADMONITOR_H

#include "log.h"

#include <atomic>
#include <fstream>
#include <mutex>
#include <string>
#include <chrono>
#include <thread>
#include <utility>

class NPULoadMonitor
{
public:
    explicit NPULoadMonitor(std::string path = "/sys/kernel/debug/rknpu/load",
                            int         npu_core_len = 3)
    : file_path_(std::move(path))
    , npu_core_len(npu_core_len)
    {
    }

    // Start monitoring in current thread (blocks, run in a separate thread)
    void start(int interval_us = 10000)
    {
        if (!open_file())
        {
            return;
        }

        running_ = true;
        while (running_)
        {
            int info[128] = {0};
            if (read_load(info))
            {
                std::lock_guard<std::mutex> lock(mtx_);
                for (int i = 0; i < npu_core_len; ++i)
                {
                    current_load[i] = info[i];
                }
            }
            std::this_thread::sleep_for(std::chrono::microseconds(interval_us));
        }
        file_.close();
    }

    void stop()
    {
        running_ = false;
    }

    // Convenience: get load for a specific core [0,1,2]
    int get_core_load(int core)
    {
        if (core < 0 || core > 2)
        {
            return 0;
        }
        std::lock_guard<std::mutex> lock(mtx_);
        return current_load[core];
    }

private:
    bool open_file()
    {
        file_.open(file_path_);
        if (!file_.is_open())
        {
            LOG_DEBUG("Failed to open %s. Try sudo or check debugfs.",
                      file_path_.c_str());
            return false;
        }
        return true;
    }

    // Parse sysfs format, e.g.:
    //   "NPU load:  Core0: 45%, Core1: 30%, Core2: 10%"
    //   or: "45 30 10"
    // Returns true if at least one value was parsed.
    bool read_load(int info[])
    {
        if (!file_.is_open())
        {
            return false;
        }

        file_.clear();
        file_.seekg(0, std::ios::beg);
        std::string line;
        if (!std::getline(file_, line))
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
                            int load_value = std::stoi(num_str);
                            info[core_id]  = load_value;
                        }
                    }
                }
            }
            pos += 9; // 跳过已找到的 "Core" 避免死循环
        }
        return true;
    }

    std::string       file_path_;
    std::ifstream     file_;
    std::atomic<bool> running_{false};

    std::mutex        mtx_;

    int               current_load[128] = {0};

    int               npu_core_len      = 3;
};

#endif // RKDETECTOR_NPULOADMONITOR_H
