#ifndef RUNE_LOGGER_H
#define RUNE_LOGGER_H

#include "utils.h"

#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <string>

namespace rune {

enum class LogLevel
{
    VERBOSE,
    DEBUG,
    INFO,
    WARN,
    ERROR,
    FATAL
};

constexpr const char* log_level_to_string(LogLevel level) {
    switch (level) {
    case LogLevel::VERBOSE:
        return "verbose";
    case LogLevel::DEBUG:
        return "debug";
    case LogLevel::INFO:
        return "info";
    case LogLevel::WARN:
        return "warn";
    case LogLevel::ERROR:
        return "error";
    case LogLevel::FATAL:
        return "fatal";
    }
    return "unknown";
}

class Logger {
  public:
    template <LogLevel level, typename... Args> void log(const char* format, const Args&... args) {
        constexpr const char* prefix = log_level_to_string(level);

        if constexpr (level >= LogLevel::ERROR) {
            log_generic(std::cerr, prefix, format, args...);
        } else {
            log_generic(std::cout, prefix, format, args...);
        }

        if constexpr (level == LogLevel::FATAL) {
            std::exit(1);
        }
    }

    template <typename... Args> void verbose(const char* format, const Args&... args) {
        log<LogLevel::VERBOSE>(format, args...);
    }

    template <typename... Args> void debug(const char* format, const Args&... args) {
        log<LogLevel::DEBUG>(format, args...);
    }

    template <typename... Args> void info(const char* format, const Args&... args) {
        log<LogLevel::INFO>(format, args...);
    }

    template <typename... Args> void warn(const char* format, const Args&... args) {
        log<LogLevel::WARN>(format, args...);
    }

    template <typename... Args> void error(const char* format, const Args&... args) {
        log<LogLevel::ERROR>(format, args...);
    }

    template <typename... Args> void fatal(const char* format, const Args&... args) {
        log<LogLevel::FATAL>(format, args...);
    }

  private:
    template <typename... Args>
    void log_generic(std::ostream& stream, const char* channel_name, const char* format, const Args&... args) {
        stream << "[";

        std::time_t t = std::time(nullptr);
#if defined(OS_WINDOWS)
        std::tm tm;
        localtime_s(&tm, &t);
        stream << std::put_time(&tm, "%F %T");
#else
        std::tm* tm = localtime(&t);
        stream << std::put_time(tm, "%F %T");
#endif

        stream << "][" << channel_name << "] " << utils::format_str(format, args...) << std::endl;
    }
};

} // namespace rune

#endif // RUNE_LOGGER_H
