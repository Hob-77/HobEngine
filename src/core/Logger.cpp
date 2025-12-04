#include "core/Logger.h"

namespace Engine
{
	std::ofstream Logger::s_logFile;
	LogLevel Logger::s_currentLevel = LogLevel::Info;

	void Logger::init(const std::string& filename)
	{
		s_logFile.open(filename);
		if (!s_logFile.is_open())
		{
			std::cerr << "Failed to open log file: " << filename << "\n";
		}
		LOG_INFO("Logger initialized");
	}

	void Logger::shutdown()
	{
		LOG_INFO("Logger shutting down");
		if (s_logFile.is_open())
		{
			s_logFile.close();
		}
	}
}