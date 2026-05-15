#include <gtest/gtest.h>

#include "services/memory/pool_manager.hpp"
#include "services/memory/stack_allocater.hpp"
#include "services/render/mesh.hpp"
#include "services/render/model.hpp"
#include "utils/exception.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <fstream>
#include <string>

namespace
{
    using MeshT = Service::Mesh<VertexTypes::Vertex1P1N1UV>;

    // Same hidden-context helper as mesh_test: Model's ctor builds Mesh
    // objects (glGen*) and its dtor releases them (glDelete*), so a current
    // GL context is required. Headless => ok() is false and GL tests skip.
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
            win_ = glfwCreateWindow(8, 8, "model_test", nullptr, nullptr);
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

    // Minimal single-triangle Wavefront OBJ. assimp's OBJ importer yields
    // 1 mesh, 1 (default) material and a root node with 1 mesh instance.
    std::string writeTriangleObj()
    {
        std::string path = ::testing::TempDir() + "papo_model_test_tri.obj";
        std::ofstream out(path, std::ios::trunc);
        out << "v 0.0 0.0 0.0\n"
               "v 1.0 0.0 0.0\n"
               "v 0.0 1.0 0.0\n"
               "f 1 2 3\n";
        return path;
    }
} // namespace

// Friend of Service::Model and Service::StackAllocater (see the
// `friend class ::ModelTest;` decls in model.hpp / stack_allocater.hpp):
// lets the fixture construct a StackAllocater and inspect Model internals
// plus drive the private Model::Load.
class ModelTest : public ::testing::Test
{
protected:
    static constexpr std::uint32_t STACK_SIZE = 8192;
    static constexpr std::uint32_t MODEL_ID = 42;

    Memory::PoolManager pool_;
    std::string objPath_;
    // Owned by the fixture (a friend of StackAllocater); its private
    // ctor/dtor are inaccessible from the TEST_F-generated subclass.
    // Only mallocs in its ctor, so it is safe even on the headless skip path.
    Service::StackAllocater stack_{STACK_SIZE};

    void SetUp() override
    {
        if (!GLEnv::ok())
            GTEST_SKIP() << "no GL context (headless); skipping GL-dependent test";

        pool_.registerPool<MeshT>(16);
        pool_.registerPool<Service::Material>(16);
        objPath_ = writeTriangleObj();
    }

    // private Model accessors
    std::size_t meshCount(const Service::Model &m) { return m.meshCount_; }
    std::size_t materialCount(const Service::Model &m) { return m.materialCount_; }
    std::size_t instanceCount(const Service::Model &m) { return m.instanceCount_; }
    bool loaded(const Service::Model &m) { return m.loaded_; }
    std::uint32_t transientMarker(const Service::Model &m) { return m.transientMarker_; }
    Service::StackAllocater *stackOf(const Service::Model &m) { return m.stack_; }
    std::uint32_t doLoad(Service::Model &m, bool clean) { return m.Load(clean); }
};

// --- construction failure (headless-safe: assimp fails before any Mesh /
//     GL call, and the throwing ctor never runs ~Model) ---

TEST(ModelCtorFailure, InvalidFilepathThrows)
{
    Memory::PoolManager pool;
    EXPECT_THROW(
        Service::Model(pool, nullptr, "definitely/not/a/real/model.obj", 0),
        Util::PapoException);
}

// --- construction (needs GL: builds Mesh objects) ---

TEST_F(ModelTest, ConstructFromTriangleObjPopulatesCounts)
{
    Service::Model model(pool_, &stack_, objPath_, MODEL_ID);

    EXPECT_EQ(meshCount(model), 1u);
    EXPECT_EQ(materialCount(model), 1u);
    EXPECT_EQ(instanceCount(model), 1u);
    EXPECT_FALSE(loaded(model));
    EXPECT_EQ(model.id(), MODEL_ID);
    EXPECT_EQ(stackOf(model), &stack_);
}

TEST_F(ModelTest, ConstructCapturesMarkerBeforeAllocatingScratch)
{
    Service::Model model(pool_, &stack_, objPath_, MODEL_ID);

    // Marker captured before any stack alloc; the fresh stack started at 0.
    EXPECT_EQ(transientMarker(model), 0u);
    // Scratch vertex/index buffers were then pushed onto the stack.
    EXPECT_GT(stack_.getMarker(), transientMarker(model));
}

// --- Load (GPU upload + optional stack reclamation) ---

TEST_F(ModelTest, LoadWithoutCleanUploadsAndKeepsStack)
{
    Service::Model model(pool_, &stack_, objPath_, MODEL_ID);

    EXPECT_EQ(doLoad(model, /*cleanCPUData=*/false), MODEL_ID);
    EXPECT_TRUE(loaded(model));
    EXPECT_EQ(stackOf(model), &stack_); // stack retained when not cleaned
}

TEST_F(ModelTest, LoadWithCleanReclaimsStackAndDetaches)
{
    Service::Model model(pool_, &stack_, objPath_, MODEL_ID);

    EXPECT_EQ(doLoad(model, /*cleanCPUData=*/true), MODEL_ID);
    EXPECT_TRUE(loaded(model));
    EXPECT_EQ(stack_.getMarker(), transientMarker(model)); // scratch reclaimed
    EXPECT_EQ(stackOf(model), nullptr);                   // detached from stack
}

TEST_F(ModelTest, LoadAfterCleanThrows)
{
    Service::Model model(pool_, &stack_, objPath_, MODEL_ID);

    ASSERT_EQ(doLoad(model, true), MODEL_ID);
    // stack_ is now null => the no-stack guard fires.
    EXPECT_THROW(doLoad(model, false), Util::PapoException);
}

TEST_F(ModelTest, RepeatedLoadWithoutCleanIsIdempotent)
{
    Service::Model model(pool_, &stack_, objPath_, MODEL_ID);

    EXPECT_EQ(doLoad(model, false), MODEL_ID);
    EXPECT_EQ(doLoad(model, false), MODEL_ID); // loaded_ guard => safe no-op
    EXPECT_TRUE(loaded(model));
}
