#pragma once

#include <memory>

#include <spdlog/spdlog.h>

class Log final
{
public:
	static void Initialize();
	static void Shutdown();

	static std::shared_ptr<spdlog::logger>& GetLogger() { return s_Logger; }

private:
	static std::shared_ptr<spdlog::logger> s_Logger;
};

#if defined(BF_DIST)

#define BF_TRACE(...)    ((void)0)
#define BF_INFO(...)     ((void)0)
#define BF_WARN(...)     ((void)0)
#define BF_ERROR(...)    ((void)0)
#define BF_CRITICAL(...) ((void)0)

#else

#define BF_TRACE(...)    SPDLOG_LOGGER_CALL(::Log::GetLogger(), spdlog::level::trace, __VA_ARGS__)
#define BF_INFO(...)     SPDLOG_LOGGER_CALL(::Log::GetLogger(), spdlog::level::info, __VA_ARGS__)
#define BF_WARN(...)     SPDLOG_LOGGER_CALL(::Log::GetLogger(), spdlog::level::warn, __VA_ARGS__)
#define BF_ERROR(...)    SPDLOG_LOGGER_CALL(::Log::GetLogger(), spdlog::level::err, __VA_ARGS__)
#define BF_CRITICAL(...) SPDLOG_LOGGER_CALL(::Log::GetLogger(), spdlog::level::critical, __VA_ARGS__)

#endif
