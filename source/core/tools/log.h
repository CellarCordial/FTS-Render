#ifndef CORE_LOG_H
#define CORE_LOG_H

#include <spdlog/spdlog.h>
#include <cassert>

static std::string log_string(std::string str, std::string file_name, uint32_t line)
{
    std::stringstream ss;
    ss << str << "   [File: " << file_name << "(" << line << ")]";
    return ss.str();
}

#ifndef LOG_INFO
#define LOG_INFO(x)	spdlog::info(log_string(x, __FILE__, __LINE__));
#endif
#ifndef LOG_WARN
#define LOG_WARN(x)	spdlog::warn(log_string(x, __FILE__, __LINE__));
#endif
#ifndef LOG_ERROR
#define LOG_ERROR(x) spdlog::error(log_string(x, __FILE__, __LINE__));
#endif
#ifndef LOG_CRITICAL
#define LOG_CRITICAL(x)	spdlog::critical(log_string(x, __FILE__, __LINE__));
#endif


#ifdef DEBUG 
#define ReturnIfFalse(bool)      \
        if (!(bool)) { LOG_ERROR(#bool); return false; }
#else
#define ReturnIfFalse(bool) (void)(bool)
#endif

#endif
