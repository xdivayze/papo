#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include "utils/exception.hpp"
#include "utils/exception_handler.hpp"
#include "services/services.hpp"

namespace fs = std::filesystem;

TEST(PapoExceptionTest, StoresTagAndMessage)
{
    Util::PapoException e("TAG", "something went wrong");
    EXPECT_EQ(e.tag_, "TAG");
    EXPECT_EQ(e.msg_, "something went wrong");
}

TEST(PapoExceptionTest, WhatReturnsCombinedString)
{
    Util::PapoException e("TAG", "something went wrong");
    EXPECT_STREQ(e.what(), "TAG: something went wrong");
}

TEST(PapoExceptionTest, IsCatchableAsStdException)
{
    EXPECT_THROW(
        {
            try { throw Util::PapoException("TAG", "msg"); }
            catch (const std::exception &) { throw; }
        },
        std::exception);
}

TEST(MemoryExceptionTest, StoresTagAndMessage)
{
    Util::MemoryException e("MEM", "out of bounds");
    EXPECT_EQ(e.tag_, "MEM");
    EXPECT_EQ(e.msg_, "out of bounds");
}

TEST(MemoryExceptionTest, WhatReturnsCombinedString)
{
    Util::MemoryException e("MEM", "out of bounds");
    EXPECT_STREQ(e.what(), "MEM: out of bounds");
}

TEST(MemoryExceptionTest, IsCatchableAsPapoException)
{
    EXPECT_THROW(
        {
            try { throw Util::MemoryException("MEM", "msg"); }
            catch (const Util::PapoException &) { throw; }
        },
        Util::PapoException);
}

TEST(MemoryExceptionTest, IsCatchableAsStdException)
{
    EXPECT_THROW(
        {
            try { throw Util::MemoryException("MEM", "msg"); }
            catch (const std::exception &) { throw; }
        },
        std::exception);
}

TEST(MemoryExceptionTest, DynamicCastFromStdException)
{
    try
    {
        throw Util::MemoryException("MEM", "msg");
    }
    catch (const std::exception &e)
    {
        EXPECT_NE(dynamic_cast<const Util::PapoException *>(&e), nullptr);
        EXPECT_NE(dynamic_cast<const Util::MemoryException *>(&e), nullptr);
    }
}

class ExceptionHandlerTest : public ::testing::Test
{
protected:
    const std::string testLogFile = "exception_handler_test.log";

    void SetUp() override
    {
        fs::remove(testLogFile);
        Engine::Root::get().logger->setOutputFile(testLogFile);
        Engine::Root::get().logger->setLogLevel(LogLevel::Error);
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

TEST_F(ExceptionHandlerTest, LogsPapoExceptionWithItsOwnTag)
{
    Util::PapoException e("MY_TAG", "my message");
    Util::handleException(Engine::Root::get().logger, e);

    std::string log = readLogFile();
    EXPECT_NE(log.find("MY_TAG"), std::string::npos);
    EXPECT_NE(log.find("my message"), std::string::npos);
}

TEST_F(ExceptionHandlerTest, LogsMemoryExceptionWithItsOwnTag)
{
    Util::MemoryException e("MEM", "out of bounds");
    Util::handleException(Engine::Root::get().logger, e);

    std::string log = readLogFile();
    EXPECT_NE(log.find("MEM"), std::string::npos);
    EXPECT_NE(log.find("out of bounds"), std::string::npos);
}

TEST_F(ExceptionHandlerTest, LogsStdExceptionWithHandlerTag)
{
    std::runtime_error e("something failed");
    Util::handleException(Engine::Root::get().logger, e);

    std::string log = readLogFile();
    EXPECT_NE(log.find("EXCEPTION HANDLER"), std::string::npos);
    EXPECT_NE(log.find("something failed"), std::string::npos);
}

TEST_F(ExceptionHandlerTest, PapoExceptionDoesNotUseHandlerTag)
{
    Util::PapoException e("MY_TAG", "msg");
    Util::handleException(Engine::Root::get().logger, e);

    std::string log = readLogFile();
    EXPECT_EQ(log.find("EXCEPTION HANDLER"), std::string::npos);
}

TEST(ExceptionHandlerInstallTest, InstallTerminateHandlerSetsTerminate)
{
    auto before = std::get_terminate();
    Util::installTerminateHandler(Engine::Root::get().logger);
    EXPECT_NE(std::get_terminate(), before);
}
