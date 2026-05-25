#ifndef _RKDETECTOR_LOG_H_
#define _RKDETECTOR_LOG_H_

#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>

#ifdef __cplusplus
#include <mutex>
#endif

enum log_level_t
{
    LOG_LVL_ERROR = 1,
    LOG_LVL_WARN  = 2,
    LOG_LVL_INFO  = 3,
    LOG_LVL_DEBUG = 4
};

#ifndef LOG_LEVEL
#ifdef NDEBUG
#define LOG_LEVEL LOG_LVL_INFO
#else
#define LOG_LEVEL LOG_LVL_DEBUG
#endif
#endif

#ifndef LOG_COLOR
#define LOG_COLOR 1
#endif

static enum log_level_t g_log_level = (enum log_level_t)LOG_LEVEL;

static inline void      LOG_SET_LEVEL(enum log_level_t level)
{
    g_log_level = level;
}

static inline const char* log_level_str(enum log_level_t level)
{
    switch (level)
    {
    case LOG_LVL_ERROR:
        return "ERR";
    case LOG_LVL_WARN:
        return "WAN";
    case LOG_LVL_INFO:
        return "INF";
    case LOG_LVL_DEBUG:
        return "DEB";
    default:
        return "   ";
    }
}

static inline const char* log_level_color(enum log_level_t level)
{
    switch (level)
    {
    case LOG_LVL_ERROR:
        return "\033[1;31m"; /* bold red */
    case LOG_LVL_WARN:
        return "\033[1;33m"; /* bold yellow */
    case LOG_LVL_INFO:
        return "\033[1;32m"; /* bold green */
    case LOG_LVL_DEBUG:
        return "\033[1;37m"; /* bold white */
    default:
        return "\033[0m";
    }
}

__attribute__((format(printf, 4, 5))) static inline void log_log(
    enum log_level_t level, const char* file, int line, const char* fmt, ...)
{
    if (level > g_log_level)
    {
        return;
    }

    /* Format entire message into buffer first */
    char    buf[1024];
    int     off = 0;

    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm tm_info;
    localtime_r(&tv.tv_sec, &tm_info);

    char time_buf[24];
    strftime(time_buf, sizeof(time_buf), "%H:%M:%S", &tm_info);

    /* strip path, keep filename only */
    const char* filename = file;
    const char* p        = file;
    while (*p)
    {
        if (*p == '/' || *p == '\\')
        {
            filename = p + 1;
        }
        p++;
    }

    int use_color = LOG_COLOR && isatty(STDERR_FILENO);

    off += snprintf(buf + off, sizeof(buf) - off, "[%s.%06ld] ", time_buf, tv.tv_usec);

    if (use_color)
    {
        off += snprintf(buf + off, sizeof(buf) - off,
                        "%s[%s]%s",
                        log_level_color(level),
                        log_level_str(level),
                        "\033[0m");
    }
    else
    {
        off += snprintf(buf + off, sizeof(buf) - off, "[%s]", log_level_str(level));
    }
#ifndef NDEBUG
    off += snprintf(buf + off, sizeof(buf) - off, " [%s:%d] ", filename, line);
#else
    (void)filename;
    (void)line;
#endif

    va_list args;
    va_start(args, fmt);
    off += vsnprintf(buf + off, sizeof(buf) - off, fmt, args);
    va_end(args);

    off += snprintf(buf + off, sizeof(buf) - off, "\n");

    /* Single atomic write under lock */
#ifdef __cplusplus
    static std::mutex log_mtx;
    std::lock_guard<std::mutex> lock(log_mtx);
#endif
    fwrite(buf, 1, off, stderr);
}

#define LOG_ERROR(...) log_log(LOG_LVL_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...)  log_log(LOG_LVL_WARN, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...)  log_log(LOG_LVL_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_DEBUG(...) log_log(LOG_LVL_DEBUG, __FILE__, __LINE__, __VA_ARGS__)

#endif /* _RKDETECTOR_LOG_H_ */
