#include <gtest/gtest.h>

#include "services/async/thread_pool_manager.hpp"
#include "services/memory/memory_manager.hpp"
#include "services/memory/pool_manager.hpp"
#include "services/render/asset_manager.hpp"
#include "utils/exception.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <filesystem>
#include <fstream>
#include <memory>
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

    // Stage a shader where AssetManager::loadShader expects it: the VFS maps a
    // shader name to "<prefix><name><extension>" (i.e. "shaders/<name>.glsl"),
    // so write the file directly under <vfsRoot>/shaders/.
    void writeVfsShader(const std::filesystem::path &vfsRoot,
                        const std::string &name, const std::string &source)
    {
        std::filesystem::path dir = vfsRoot / "shaders";
        std::filesystem::create_directories(dir);
        std::ofstream(dir / (name + ".glsl"), std::ios::trunc) << source;
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
    // AssetManager stores the VFS by reference, so it must outlive every
    // AssetManager built in the test bodies; the fixture owns it. allowDangerous
    // lets reads/writes resolve inside the root without an allowed-path list.
    std::unique_ptr<Service::VFS> vfs_;
    std::filesystem::path vfsRoot_;
    std::string pathA_;
    std::string pathB_;

    void SetUp() override
    {
        if (!GLEnv::ok())
            GTEST_SKIP() << "no GL context (headless); skipping GL-dependent test";

        Service::MemoryManager::MemoryManagerContext ctx{STACK_SIZE};
        mm_ = new Service::MemoryManager(&ctx);
        tp_ = new async::ThreadPoolManager(2); // private ctor: fixture is a friend

        const auto *info = ::testing::UnitTest::GetInstance()->current_test_info();
        vfsRoot_ = std::filesystem::path(::testing::TempDir()) /
                   (std::string("papo_am_vfs_") + info->name());
        std::filesystem::remove_all(vfsRoot_); // clear any stale state
        vfs_ = std::make_unique<Service::VFS>(
            std::vector<std::filesystem::path>{}, /*allowDangerous=*/true,
            vfsRoot_);

        pathA_ = writeTriangleObj("papo_am_test_a.obj");
        pathB_ = writeTriangleObj("papo_am_test_b.obj");
    }

    void TearDown() override
    {
        vfs_.reset(); // ~VFS removes vfsRoot_
        delete tp_;   // private dtor reachable: fixture is a friend
        delete mm_;
    }
};

TEST_F(AssetManagerTest, ModelFromFilePathReturnsValidHandle)
{
    Service::AssetManager am(pool_, *mm_, /*nstacks=*/2, *vfs_);

    auto h = am.modelFromFilePath(pathA_);
    EXPECT_TRUE(h.isValid());
    EXPECT_NE(h.ptr, nullptr);
    EXPECT_EQ(h.ptr->id(), h.id);
}

TEST_F(AssetManagerTest, SamePathIsCachedAndReused)
{
    Service::AssetManager am(pool_, *mm_, /*nstacks=*/2, *vfs_);

    auto first = am.modelFromFilePath(pathA_);
    auto second = am.modelFromFilePath(pathA_);

    EXPECT_EQ(first.id, second.id);   // no new id consumed
    EXPECT_EQ(first.ptr, second.ptr); // same cached object
}

TEST_F(AssetManagerTest, DistinctPathsGetDistinctModels)
{
    Service::AssetManager am(pool_, *mm_, /*nstacks=*/2, *vfs_);

    auto a = am.modelFromFilePath(pathA_);
    auto b = am.modelFromFilePath(pathB_);

    EXPECT_NE(a.id, b.id);
    EXPECT_NE(a.ptr, b.ptr);
}

TEST_F(AssetManagerTest, LoadToGPUSucceeds)
{
    Service::AssetManager am(pool_, *mm_, /*nstacks=*/2, *vfs_);

    auto h = am.modelFromFilePath(pathA_);
    EXPECT_NO_THROW(am.loadModelToGPU(h, /*cleanLastCPUData=*/false));
}

TEST_F(AssetManagerTest, SingleStackIsRecycledAfterCleanUpload)
{
    // nstacks == 1: the second (distinct-path) load can only proceed if the
    // first lease was returned by the clean GPU upload. A broken return path
    // would block here forever.
    Service::AssetManager am(pool_, *mm_, /*nstacks=*/1, *vfs_);

    auto a = am.modelFromFilePath(pathA_);
    am.loadModelToGPU(a, /*cleanLastCPUData=*/true);

    auto b = am.modelFromFilePath(pathB_);
    EXPECT_TRUE(b.isValid());
    EXPECT_NE(a.id, b.id);
}

TEST_F(AssetManagerTest, BadFilepathThrowsAndDoesNotLeakLease)
{
    Service::AssetManager am(pool_, *mm_, /*nstacks=*/1, *vfs_);

    EXPECT_THROW(am.modelFromFilePath(::testing::TempDir() + "does_not_exist.obj"),
                 Util::PapoException);

    // The single lease must have been returned: a valid load still works.
    auto h = am.modelFromFilePath(pathA_);
    EXPECT_TRUE(h.isValid());
}

// --- async variant ---

TEST_F(AssetManagerTest, AsyncBadFilepathFutureRethrows)
{
    Service::AssetManager am(pool_, *mm_, /*nstacks=*/1, *vfs_);

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
    Service::AssetManager am(pool_, *mm_, /*nstacks=*/2, *vfs_);

    auto fut = am.modelFromFilePathAsync(*tp_, pathA_);
    auto h = fut.get(); // built on a worker thread, no GL there
    ASSERT_TRUE(h.isValid());

    // loadModelToGPU lazily initializes the deferred buffers on this thread.
    EXPECT_NO_THROW(am.loadModelToGPU(h, /*cleanLastCPUData=*/true));
}

TEST_F(AssetManagerTest, ExplicitInitializeMeshBuffersThenUpload)
{
    Service::AssetManager am(pool_, *mm_, /*nstacks=*/2, *vfs_);

    auto fut = am.modelFromFilePathAsync(*tp_, pathA_);
    auto h = fut.get();
    ASSERT_TRUE(h.isValid());

    // Explicit GL-thread initialization, separate from the upload step.
    EXPECT_NO_THROW(h.ptr->initializeMeshBuffers());
    EXPECT_NO_THROW(am.loadModelToGPU(h, /*cleanLastCPUData=*/false));
}

TEST_F(AssetManagerTest, AsyncCacheHitReturnsSameHandle)
{
    Service::AssetManager am(pool_, *mm_, /*nstacks=*/2, *vfs_);

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

// --- loadShader (VFS-backed) ---

TEST_F(AssetManagerTest, LoadShaderReadsSourceFromVFS)
{
    Service::AssetManager am(pool_, *mm_, /*nstacks=*/2, *vfs_);

    const std::string source =
        "#version 460 core\nvoid main() { gl_Position = vec4(0.0); }\n";
    writeVfsShader(vfsRoot_, "basic", source);

    EXPECT_EQ(am.loadShader("basic"), source);
}

TEST_F(AssetManagerTest, LoadShaderResolvesNameToShaderDirAndExtension)
{
    Service::AssetManager am(pool_, *mm_, /*nstacks=*/2, *vfs_);

    // The name must be mapped to "shaders/<name>.glsl": a file with the same
    // stem but a different location/extension must NOT be picked up.
    writeVfsShader(vfsRoot_, "lit", "CORRECT");
    std::ofstream(vfsRoot_ / "lit.glsl", std::ios::trunc) << "WRONG_NO_PREFIX";

    EXPECT_EQ(am.loadShader("lit"), "CORRECT");
}

TEST_F(AssetManagerTest, LoadShaderDistinctNamesReadDistinctSources)
{
    Service::AssetManager am(pool_, *mm_, /*nstacks=*/2, *vfs_);

    writeVfsShader(vfsRoot_, "vert", "VERTEX_SRC");
    writeVfsShader(vfsRoot_, "frag", "FRAGMENT_SRC");

    EXPECT_EQ(am.loadShader("vert"), "VERTEX_SRC");
    EXPECT_EQ(am.loadShader("frag"), "FRAGMENT_SRC");
}

TEST_F(AssetManagerTest, LoadShaderMissingReturnsEmpty)
{
    Service::AssetManager am(pool_, *mm_, /*nstacks=*/2, *vfs_);

    // readAll on a non-existent file yields an empty string (the read handle
    // simply fails to open); loadShader surfaces that as "".
    EXPECT_EQ(am.loadShader("does_not_exist"), "");
}
