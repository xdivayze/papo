#include <gtest/gtest.h>
#include "logger/logger.hpp"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

class LoggerTest : public ::testing::Test
{
protected:
    const std::string testLogFile = "test_output.log";

    void SetUp() override
    {
        fs::remove(testLogFile);
        Engine::Logger::get().setOutputFile(testLogFile);
        Engine::Logger::get().setLogLevel(LogLevel::Info);
    }

    void TearDown() override
    {
        fs::remove(testLogFile);
    }

    std::string readLogFile()
    {
        std::ifstream f(testLogFile);
        return std::string(std::istreambuf_iterator<char>(f), {});
    }
};

TEST_F(LoggerTest, SetOutputFileSucceeds)
{
    EXPECT_EQ(Engine::Logger::get().setOutputFile(testLogFile), 0);
}

TEST_F(LoggerTest, SetOutputFileFailsOnInvalidPath)
{
    EXPECT_EQ(Engine::Logger::get().setOutputFile("/nonexistent/dir/test.log"), -1);
}

TEST_F(LoggerTest, LogLevelFiltersMessages)
{
    Engine::Logger::get().setLogLevel(LogLevel::Error);
    Engine::Logger::get().info("TEST", "should be filtered");

    std::string content = readLogFile();
    EXPECT_TRUE(content.empty());
}

TEST_F(LoggerTest, LogLevelAllowsMessagesAtOrAboveMinLevel)
{
    Engine::Logger::get().setLogLevel(LogLevel::Warning);
    Engine::Logger::get().warn("TEST", "visible warning");
    Engine::Logger::get().error("TEST", "visible error");

    std::string content = readLogFile();
    EXPECT_FALSE(content.empty());
}

TEST_F(LoggerTest, InfoMessageIsWritten)
{
    Engine::Logger::get().setLogLevel(LogLevel::Info);
    Engine::Logger::get().info("TEST", "hello info");

    std::string content = readLogFile();
    EXPECT_FALSE(content.empty());
}

TEST_F(LoggerTest, GetReturnsSameInstance)
{
    EXPECT_EQ(&Engine::Logger::get(), &Engine::Logger::get());
}
