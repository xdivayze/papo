#include "services/logger.hpp"
#include <chrono>
#include <format>
#include <stdexcept>

static constexpr const char *DEFAULT_LOG_FILE = "engine.log";
static constexpr const char *TAG = "LOGGER";
static constexpr const char *levelToString(LogLevel level)
{
    switch (level)
    {
    case LogLevel::Info:

        return "INFO";
    case LogLevel::Error:
        return "ERROR";
    case LogLevel::Warning:
        return "WARN";
    }
}

namespace Services
{
    Logger::Logger()
    {
        path_ = DEFAULT_LOG_FILE;
        file_ = std::fopen(DEFAULT_LOG_FILE, "a");
        if (!file_)
        {
            throw std::runtime_error(std::string("Logger: failed to open file: ") + DEFAULT_LOG_FILE);
        }
        minLevel_ = LogLevel::Info;
    }

    Logger::~Logger()
    {
        if (file_)
        {
            if (std::fclose(file_) == EOF)
            {
                Logger::error(TAG, "failed to close the filestream");
            }
        }
    }

    int Logger::setOutputFile(const std::string &filepath)
    {
        FILE *fileTemp = std::fopen(filepath.c_str(), "a");
        if (!fileTemp)
        {
            Logger::error(TAG, "couldn't set output file, function returned NULL.");
            return -1;
        }

        if (file_)
        {
            std::fclose(file_);
        }

        file_ = fileTemp;
        path_ = filepath;
        return 0;
    }

    void Logger::setLogLevel(LogLevel level)
    {
        minLevel_ = level;
    }

    void Logger::warn(const std::string &tag, const std::string &msg)
    {
        log(tag, msg, LogLevel::Warning);
    }

    void Logger::info(const std::string &tag, const std::string &msg)
    {
        log(tag, msg, LogLevel::Info);
    }

    void Logger::error(const std::string &tag, const std::string &msg)
    {
        log(tag, msg, LogLevel::Error);
    }

    void Logger::log(const std::string &tag, const std::string &msg, LogLevel level)
    {
        if (level >= minLevel_)
        {
            writeFormatted(level, msg, tag);
        }
    }

    void Logger::writeFormatted(LogLevel level, const std::string &msg, const std::string &tag)
    {

        std::string formattedString;
        formattedString = std::format("[{:%H:%M:%S}] [{}] [{}] {}\n", std::chrono::zoned_time{std::chrono::current_zone(), std::chrono::system_clock::now()}, levelToString(level), tag, msg);
        std::fwrite(formattedString.data(), sizeof(char), formattedString.size(), file_);
        std::fflush(file_);
    }
}