#include <gtest/gtest.h>
#include "services/services.hpp"
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
        Engine::Root::get().logger.setOutputFile(testLogFile);
        Engine::Root::get().logger.setLogLevel(LogLevel::Info);
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
    EXPECT_EQ(Engine::Root::get().logger.setOutputFile(testLogFile), 0);
}

TEST_F(LoggerTest, SetOutputFileFailsOnInvalidPath)
{
    EXPECT_EQ(Engine::Root::get().logger.setOutputFile("/nonexistent/dir/test.log"), -1);
}

TEST_F(LoggerTest, LogLevelFiltersMessages)
{
    Engine::Root::get().logger.setLogLevel(LogLevel::Error);
    Engine::Root::get().logger.info("TEST", "should be filtered");

    std::string content = readLogFile();
    EXPECT_TRUE(content.empty());
}

TEST_F(LoggerTest, LogLevelAllowsMessagesAtOrAboveMinLevel)
{
    Engine::Root::get().logger.setLogLevel(LogLevel::Warning);
    Engine::Root::get().logger.warn("TEST", "visible warning");
    Engine::Root::get().logger.error("TEST", "visible error");

    std::string content = readLogFile();
    EXPECT_FALSE(content.empty());
}

TEST_F(LoggerTest, InfoMessageIsWritten)
{
    Engine::Root::get().logger.setLogLevel(LogLevel::Info);
    Engine::Root::get().logger.info("TEST", "hello info");

    std::string content = readLogFile();
    EXPECT_FALSE(content.empty());
}

TEST_F(LoggerTest, GetReturnsSameInstance)
{
    EXPECT_EQ(&Engine::Root::get().logger, &Engine::Root::get().logger);
}
