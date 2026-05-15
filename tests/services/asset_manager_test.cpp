#include <gtest/gtest.h>

#include "services/async/thread_pool_manager.hpp"
#include "services/memory/memory_manager.hpp"
#include "services/memory/pool_manager.hpp"
#include "services/render/asset_manager.hpp"
#include "utils/exception.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <fstream>
#include <string>

namespace
{
    // Hidden GLFW window + glad-loaded context, created once per binary.
    // AssetManager builds Models (Mesh glGen*) and tears them down
    // (glDelete*), so a current GL context is required; headless => skip.
    class GLEnv
    {
    public:
        static bool ok()
        {
            static GLEnv instance;
            return instance.ok_;
        }

    private:
        GLFWwindow *win_ = nullptr;
        bool ok_ = false;

        GLEnv()
        {
            if (!glfwInit())
                return;
            glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
            win_ = glfwCreateWindow(8, 8, "asset_manager_test", nullptr, nullptr);
            if (!win_)
            {
                glfwTerminate();
                return;
            }
            glfwMakeContextCurrent(win_);
            if (!gladLoadGL(reinterpret_cast<GLADloadfunc>(glfwGetProcAddress)))
            {
                glfwDestroyWindow(win_);
                glfwTerminate();
                return;
            }
            ok_ = true;
        }

        ~GLEnv()
        {
            if (win_)
                glfwDestroyWindow(win_);
            if (ok_)
                glfwTerminate();
        }
    };

    std::string writeTriangleObj(const char *name)
    {
        std::string path = ::testing::TempDir() + name;
        std::ofstream out(path, std::ios::trunc);
        out << "v 0.0 0.0 0.0\n"
               "v 1.0 0.0 0.0\n"
               "v 0.0 1.0 0.0\n"
               "f 1 2 3\n";
        return path;
    }
} // namespace

// Friend of Service::MemoryManager (see `friend class ::AssetManagerTest;`
// in memory_manager.hpp) so the fixture can construct one (private ctor/dtor)
// to inject into AssetManager. AssetManager's own ctor/dtor are public, so
// no friendship is needed there.
class AssetManagerTest : public ::testing::Test
{
protected:
    static constexpr std::uint32_t STACK_SIZE = 1u << 20; // 1 MiB

    Memory::PoolManager pool_;
    Service::MemoryManager *mm_ = nullptr;
    async::ThreadPoolManager *tp_ = nullptr;
    std::string pathA_;
    std::string pathB_;

    void SetUp() override
    {
        if (!GLEnv::ok())
            GTEST_SKIP() << "no GL context (headless); skipping GL-dependent test";

        Service::MemoryManager::MemoryManagerContext ctx{STACK_SIZE};
        mm_ = new Service::MemoryManager(&ctx);
        tp_ = new async::ThreadPoolManager(2); // private ctor: fixture is a friend
        pathA_ = writeTriangleObj("papo_am_test_a.obj");
        pathB_ = writeTriangleObj("papo_am_test_b.obj");
    }

    void TearDown() override
    {
        delete tp_; // private dtor reachable: fixture is a friend
        delete mm_;
    }
};

TEST_F(AssetManagerTest, ModelFromFilePathReturnsValidHandle)
{
    Service::AssetManager am(pool_, *mm_, /*nstacks=*/2);

    auto h = am.modelFromFilePath(pathA_);
    EXPECT_TRUE(h.isValid());
    EXPECT_NE(h.ptr, nullptr);
    EXPECT_EQ(h.ptr->id(), h.id);
}

TEST_F(AssetManagerTest, SamePathIsCachedAndReused)
{
    Service::AssetManager am(pool_, *mm_, /*nstacks=*/2);

    auto first = am.modelFromFilePath(pathA_);
    auto second = am.modelFromFilePath(pathA_);

    EXPECT_EQ(first.id, second.id);   // no new id consumed
    EXPECT_EQ(first.ptr, second.ptr); // same cached object
}

TEST_F(AssetManagerTest, DistinctPathsGetDistinctModels)
{
    Service::AssetManager am(pool_, *mm_, /*nstacks=*/2);

    auto a = am.modelFromFilePath(pathA_);
    auto b = am.modelFromFilePath(pathB_);

    EXPECT_NE(a.id, b.id);
    EXPECT_NE(a.ptr, b.ptr);
}

TEST_F(AssetManagerTest, LoadToGPUSucceeds)
{
    Service::AssetManager am(pool_, *mm_, /*nstacks=*/2);

    auto h = am.modelFromFilePath(pathA_);
    EXPECT_NO_THROW(am.loadModelToGPU(h, /*cleanLastCPUData=*/false));
}

TEST_F(AssetManagerTest, SingleStackIsRecycledAfterCleanUpload)
{
    // nstacks == 1: the second (distinct-path) load can only proceed if the
    // first lease was returned by the clean GPU upload. A broken return path
    // would block here forever.
    Service::AssetManager am(pool_, *mm_, /*nstacks=*/1);

    auto a = am.modelFromFilePath(pathA_);
    am.loadModelToGPU(a, /*cleanLastCPUData=*/true);

    auto b = am.modelFromFilePath(pathB_);
    EXPECT_TRUE(b.isValid());
    EXPECT_NE(a.id, b.id);
}

TEST_F(AssetManagerTest, BadFilepathThrowsAndDoesNotLeakLease)
{
    Service::AssetManager am(pool_, *mm_, /*nstacks=*/1);

    EXPECT_THROW(am.modelFromFilePath(::testing::TempDir() + "does_not_exist.obj"),
                 Util::PapoException);

    // The single lease must have been returned: a valid load still works.
    auto h = am.modelFromFilePath(pathA_);
    EXPECT_TRUE(h.isValid());
}

// --- async variant ---

TEST_F(AssetManagerTest, AsyncBadFilepathFutureRethrows)
{
    Service::AssetManager am(pool_, *mm_, /*nstacks=*/1);

    // The worker fails inside the assimp parse (before any GL call), so this
    // is safe without a GL context on the pool thread. The exception must
    // surface through the future, and the lease must be returned.
    auto fut = am.modelFromFilePathAsync(
        *tp_, ::testing::TempDir() + "does_not_exist.obj");
    EXPECT_THROW(fut.get(), Util::PapoException);

    auto h = am.modelFromFilePath(pathA_);
    EXPECT_TRUE(h.isValid());
}

TEST_F(AssetManagerTest, AsyncDeferredBuildOffGLThreadThenUpload)
{
    // The payoff of the deferred-GL design: the worker builds a real Model
    // (assimp parse + pool alloc) with NO GL context, then the render thread
    // (this test thread) finalizes GL buffers + uploads.
    Service::AssetManager am(pool_, *mm_, /*nstacks=*/2);

    auto fut = am.modelFromFilePathAsync(*tp_, pathA_);
    auto h = fut.get(); // built on a worker thread, no GL there
    ASSERT_TRUE(h.isValid());

    // loadModelToGPU lazily initializes the deferred buffers on this thread.
    EXPECT_NO_THROW(am.loadModelToGPU(h, /*cleanLastCPUData=*/true));
}

TEST_F(AssetManagerTest, ExplicitInitializeMeshBuffersThenUpload)
{
    Service::AssetManager am(pool_, *mm_, /*nstacks=*/2);

    auto fut = am.modelFromFilePathAsync(*tp_, pathA_);
    auto h = fut.get();
    ASSERT_TRUE(h.isValid());

    // Explicit GL-thread initialization, separate from the upload step.
    EXPECT_NO_THROW(h.ptr->initializeMeshBuffers());
    EXPECT_NO_THROW(am.loadModelToGPU(h, /*cleanLastCPUData=*/false));
}

TEST_F(AssetManagerTest, AsyncCacheHitReturnsSameHandle)
{
    Service::AssetManager am(pool_, *mm_, /*nstacks=*/2);

    // Build it synchronously on the test thread (GL context is current here).
    auto sync = am.modelFromFilePath(pathA_);
    ASSERT_TRUE(sync.isValid());

    // The async request hits the cache and returns immediately on the worker
    // without touching GL or consuming a lease.
    auto fut = am.modelFromFilePathAsync(*tp_, pathA_);
    auto async = fut.get();

    EXPECT_EQ(async.id, sync.id);
    EXPECT_EQ(async.ptr, sync.ptr);
}
