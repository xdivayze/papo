#pragma once
#include <string>
#include <filesystem>

enum class LogLevel
{
    Info,
    Warning,
    Error
};

namespace Engine { class Root; }

namespace Services
{
    class Logger
    {
    public:
        void log(const std::string &tag, const std::string &msg, LogLevel level);
        void info(const std::string &tag, const std::string &msg);
        void error(const std::string &tag, const std::string &msg);
        void warn(const std::string &tag, const std::string &msg);

        int setOutputFile(const std::string &filepath);
        void setLogLevel(LogLevel minLevel);

        friend class Engine::Root;

    private:
        Logger();
        ~Logger();
        LogLevel minLevel_;
        FILE *file_;
        std::filesystem::path path_;
        void writeFormatted(LogLevel level, const std::string &msg, const std::string &tag);
    };

}