#include "Core/Log.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>

std::shared_ptr<spdlog::logger> Log::s_Logger;

void Log::Initialize()
{
	if (s_Logger)
		return;

	auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

	consoleSink->set_pattern("%^[%T] [%l] [%s:%#] %v%$");

	s_Logger = std::make_shared<spdlog::logger>("BLACKFROST", std::move(consoleSink));

	s_Logger->set_level(spdlog::level::trace);
	s_Logger->flush_on(spdlog::level::err);
}

void Log::Shutdown()
{
	if (!s_Logger)
		return;

	s_Logger->flush();
	s_Logger.reset();

	spdlog::shutdown();
}
