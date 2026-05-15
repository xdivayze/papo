#include <gtest/gtest.h>

#include "services/render/mesh.hpp"
#include "utils/exception.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace
{
    // A hidden GLFW window + glad-loaded context, created once for the whole
    // test binary. Mesh's ctor/Load/dtor call into GL, so without a current
    // context those tests cannot run; in that case ok() is false and the
    // GL-dependent tests GTEST_SKIP instead of crashing on null GL pointers.
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
            win_ = glfwCreateWindow(8, 8, "mesh_test", nullptr, nullptr);
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
} // namespace

using M = Service::Mesh<VertexTypes::Vertex1P>;

// Friend of Service::Mesh (see `friend class ::MeshTest;` in mesh.hpp) so the
// accessors below can read the private GL handles / data pointers. TEST_F
// bodies reach the privates only through these protected accessors.
class MeshTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        if (!GLEnv::ok())
            GTEST_SKIP() << "no GL context (headless); skipping GL-dependent test";
    }

    VertexTypes::Vertex1P *verts(const M &m) { return m.vertices_; }
    unsigned int vertexCount(const M &m) { return m.vertexCount_; }
    unsigned int *indices(const M &m) { return m.indices_; }
    unsigned int indexCount(const M &m) { return m.indexCount_; }
    unsigned int vao(const M &m) { return m.VAO_; }
    unsigned int vbo(const M &m) { return m.VBO_; }
    unsigned int ebo(const M &m) { return m.EBO_; }
    bool preLoaded(const M &m) { return m.preLoaded_; }
    void doLoad(M &m) { m.Load(); }
};

// --- ctor validation (runs even headless: the throw is before any GL call,
//     and a ctor that throws never runs the GL-touching destructor) ---

TEST(MeshCtorValidation, ZeroVertexCountThrows)
{
    VertexTypes::Vertex1P v{};
    unsigned int idx[] = {0};
    EXPECT_THROW(M(&v, 0, idx, 1), Util::PapoException);
}

TEST(MeshCtorValidation, ZeroIndexCountThrows)
{
    VertexTypes::Vertex1P v{};
    unsigned int idx[] = {0};
    EXPECT_THROW(M(&v, 1, idx, 0), Util::PapoException);
}

// --- uploading ctor (needs a GL context) ---

TEST_F(MeshTest, UploadCtorGeneratesNonZeroGLObjects)
{
    VertexTypes::Vertex1P v[3]{};
    unsigned int idx[3] = {0, 1, 2};
    M m(v, 3, idx, 3);

    EXPECT_NE(vao(m), 0u);
    EXPECT_NE(vbo(m), 0u);
    EXPECT_NE(ebo(m), 0u);
}

TEST_F(MeshTest, UploadCtorStoresVertexAndIndexData)
{
    VertexTypes::Vertex1P v[3]{};
    unsigned int idx[3] = {0, 1, 2};
    M m(v, 3, idx, 3);

    EXPECT_EQ(verts(m), v);
    EXPECT_EQ(vertexCount(m), 3u);
    EXPECT_EQ(indices(m), idx);
    EXPECT_EQ(indexCount(m), 3u);
    EXPECT_FALSE(preLoaded(m));
}

TEST_F(MeshTest, UploadCtorThenLoadDoesNotThrow)
{
    VertexTypes::Vertex1P v[3]{};
    unsigned int idx[3] = {0, 1, 2};
    M m(v, 3, idx, 3);

    EXPECT_NO_THROW(doLoad(m));
}

TEST_F(MeshTest, DistinctMeshesGetDistinctGLObjects)
{
    VertexTypes::Vertex1P v[3]{};
    unsigned int idx[3] = {0, 1, 2};
    M a(v, 3, idx, 3);
    M b(v, 3, idx, 3);

    EXPECT_NE(vao(a), vao(b));
    EXPECT_NE(vbo(a), vbo(b));
    EXPECT_NE(ebo(a), ebo(b));
}

// --- "already in GPU memory" ctor (no glGen; flag + handles only) ---

TEST_F(MeshTest, PreLoadedCtorStoresHandlesAndSetsFlag)
{
    M m(11u, 22u, 33u, 7u);

    EXPECT_EQ(vao(m), 11u);
    EXPECT_EQ(vbo(m), 22u);
    EXPECT_EQ(ebo(m), 33u);
    EXPECT_EQ(indexCount(m), 7u);
    EXPECT_TRUE(preLoaded(m));
}

TEST_F(MeshTest, PreLoadedMeshLoadDoesNotReupload)
{
    M m(11u, 22u, 33u, 7u);
    // preLoaded_ == true => Load() must skip glBufferData and not throw.
    EXPECT_NO_THROW(doLoad(m));
}

// --- externally-bound buffers ctor (7-arg: stores everything, no glGen) ---

TEST_F(MeshTest, ExternalBuffersCtorStoresAllFields)
{
    VertexTypes::Vertex1P v[2]{};
    unsigned int idx[2] = {0, 1};
    M m(v, 2, idx, 2, 5u, 6u, 7u);

    EXPECT_EQ(verts(m), v);
    EXPECT_EQ(vertexCount(m), 2u);
    EXPECT_EQ(indices(m), idx);
    EXPECT_EQ(indexCount(m), 2u);
    EXPECT_EQ(vao(m), 5u);
    EXPECT_EQ(vbo(m), 6u);
    EXPECT_EQ(ebo(m), 7u);
}

// --- destructor: releasing GL objects must not crash with a live context ---

TEST_F(MeshTest, DestructorReleasesGLObjectsWithoutCrash)
{
    VertexTypes::Vertex1P v[3]{};
    unsigned int idx[3] = {0, 1, 2};
    EXPECT_NO_THROW({
        M m(v, 3, idx, 3);
        (void)m;
    });
}
