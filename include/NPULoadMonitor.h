#ifndef RKDETECTOR_NPULOADMONITOR_H
#define RKDETECTOR_NPULOADMONITOR_H

#include <atomic>
#include <fstream>
#include <mutex>
#include <string>

struct NpuLoadInfo
{
    int load[3] = {0, 0, 0};
};

class NPULoadMonitor
{
public:
    explicit NPULoadMonitor(
        const std::string& path = "/sys/kernel/debug/rknpu/load");

    void start(int interval_us = 10000);
    void stop();

    NpuLoadInfo get_info();
    int         get_core_load(int core);

private:
    bool open_file();
    bool read_load(NpuLoadInfo& info);

    std::string       file_path_;
    std::ifstream     file_;
    std::atomic<bool> running_{false};

    std::mutex    mtx_;
    NpuLoadInfo   current_;
};

#endif // RKDETECTOR_NPULOADMONITOR_H
