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
    NPULoadMonitor(std::string path         = "/sys/kernel/debug/rknpu/load",
                   int         npu_core_len = 3);
    void start(int interval_ms = 10000);
    void stop();

    int  get_core_load(int core);

private:
    bool               read_load();

    std::ifstream      m_file;
    std::atomic<bool>  m_running{false};
    std::mutex         m_mtx;

    std::map<int, int> m_load_map;

    int                npu_core_len = 3;
};

} // namespace rkdet

#endif // RKDETECTOR_NPULOADMONITOR_H
