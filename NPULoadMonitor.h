#ifndef RKDETECTOR_NPULOADMONITOR_H
#define RKDETECTOR_NPULOADMONITOR_H

#include "log.h"

#include <atomic>
#include <fstream>
#include <mutex>
#include <string>
#include <chrono>
#include <thread>
#include <unistd.h>

struct NpuLoadInfo
{
    int load[3] = {0, 0, 0};
};

class NPULoadMonitor
{
public:
    explicit NPULoadMonitor(
        const std::string& path = "/sys/kernel/debug/rknpu/load")
    : file_path_(path)
    , running_(false)
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
            NpuLoadInfo info;
            if (read_load(info))
            {
                std::lock_guard<std::mutex> lock(mtx_);
                current_ = info;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(interval_us));
        }
        file_.close();
    }

    void stop()
    {
        running_ = false;
    }

    NpuLoadInfo get_info()
    {
        std::lock_guard<std::mutex> lock(mtx_);
        return current_;
    }

    // Convenience: get load for a specific core [0,1,2]
    int get_core_load(int core)
    {
        if (core < 0 || core > 2)
        {
            return 0;
        }
        std::lock_guard<std::mutex> lock(mtx_);
        return current_.load[core];
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
    bool read_load(NpuLoadInfo& info)
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
        int  vals[3]  = {0, 0, 0};
        int  count    = 0;
        bool in_num   = false;
        int  num      = 0;

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
            info.load[i] = vals[i];
        }
        return true;
    }

    std::string      file_path_;
    std::ifstream    file_;
    std::atomic<bool> running_{false};

    std::mutex       mtx_;
    NpuLoadInfo      current_;
};

#endif // RKDETECTOR_NPULOADMONITOR_H
