#ifndef RKDETECTOR_NPULOADMONITOR_H
#define RKDETECTOR_NPULOADMONITOR_H

#include <atomic>
#include <fstream>
#include <map>
#include <mutex>
#include <string>

namespace rkdet
{

class NPULoadMonitor
{
public:
    explicit NPULoadMonitor(
        const std::string& path = "/sys/kernel/debug/rknpu/load");

    void start(int interval_ms = 10000);
    void stop();

    int  get_core_load(int core);

private:
    bool               read_load();

    std::ifstream      m_file;
    std::atomic<bool>  m_running{};
    std::mutex         mtx_;

    std::map<int, int> m_load_map;
};

} // namespace rkdet

#endif // RKDETECTOR_NPULOADMONITOR_H
