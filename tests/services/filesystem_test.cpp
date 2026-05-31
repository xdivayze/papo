#include <gtest/gtest.h>
#include "services/filesystem.hpp"
#include "utils/exception.hpp"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace
{
    std::string writeRealFile(const fs::path &p, const std::string &contents)
    {
        fs::create_directories(p.parent_path());
        std::ofstream(p) << contents;
        return contents;
    }
} // namespace

// ============================================================
// Fixture: gives every test an isolated, unique VFS root so the
// VFS destructor (which does fs::remove_all(root_)) cannot clobber
// another test's data.
// ============================================================
class VFSTest : public ::testing::Test
{
protected:
    fs::path root_;
    fs::path sandbox_; // real directory outside the VFS root for source files

    void SetUp() override
    {
        const auto *info = ::testing::UnitTest::GetInstance()->current_test_info();
        const std::string unique =
            std::string(info->test_suite_name()) + "_" + info->name();

        root_    = fs::temp_directory_path() / ("vfs_root_" + unique);
        sandbox_ = fs::temp_directory_path() / ("vfs_sbox_" + unique);

        fs::remove_all(root_);
        fs::remove_all(sandbox_);
        fs::create_directories(sandbox_);
    }

    void TearDown() override
    {
        fs::remove_all(root_);
        fs::remove_all(sandbox_);
    }

    // A VFS that ignores the allowed-path checks (so translateVFS lets us
    // read/write freely inside the root).
    Service::VFS makeDangerousVFS()
    {
        return Service::VFS({}, /*allowDangerous=*/true, root_);
    }

    // A VFS that enforces allowed-path checks.
    Service::VFS makeStrictVFS(std::vector<fs::path> allowed = {})
    {
        return Service::VFS(std::move(allowed), /*allowDangerous=*/false, root_);
    }
};

// ============================================================
// Construction / destruction
// ============================================================

TEST_F(VFSTest, ConstructorCreatesRootDirectory)
{
    ASSERT_FALSE(fs::exists(root_));
    {
        Service::VFS vfs = makeDangerousVFS();
        EXPECT_TRUE(fs::exists(root_));
        EXPECT_TRUE(fs::is_directory(root_));
    }
}

TEST_F(VFSTest, DestructorRemovesRootDirectory)
{
    {
        Service::VFS vfs = makeDangerousVFS();
        ASSERT_TRUE(fs::exists(root_));
    }
    EXPECT_FALSE(fs::exists(root_));
}

TEST_F(VFSTest, ConstructorAcceptsPreExistingRoot)
{
    fs::create_directories(root_);
    ASSERT_TRUE(fs::exists(root_));
    EXPECT_NO_THROW({ Service::VFS vfs = makeDangerousVFS(); });
}

// ============================================================
// translateVFS
// ============================================================

TEST_F(VFSTest, TranslateVFSRejectsAbsolutePath)
{
    Service::VFS vfs = makeDangerousVFS();
    EXPECT_THROW(vfs.translateVFS("/etc/passwd"), Util::PapoException);
}

TEST_F(VFSTest, TranslateVFSRejectsEscapingPath)
{
    Service::VFS vfs = makeDangerousVFS();
    EXPECT_THROW(vfs.translateVFS("../escape.txt"), Util::PapoException);
}

TEST_F(VFSTest, TranslateVFSResolvesRelativePathInsideRoot)
{
    Service::VFS vfs = makeDangerousVFS();
    fs::path resolved = vfs.translateVFS("assets/model.obj");
    EXPECT_EQ(resolved, fs::weakly_canonical(root_ / "assets/model.obj"));
}

TEST_F(VFSTest, TranslateVFSResolvedPathIsInsideRoot)
{
    Service::VFS vfs = makeDangerousVFS();
    fs::path resolved = vfs.translateVFS("a/b/c.txt");
    EXPECT_TRUE(Service::VFS::checkIfSubdir(root_, resolved));
}

TEST_F(VFSTest, TranslateVFSEnforcesAllowedPathsWhenNotDangerous)
{
    // Strict VFS with no allowed paths: even a valid in-root virtual path
    // is rejected because the resolved path is not under any allowed dir.
    Service::VFS vfs = makeStrictVFS();
    EXPECT_THROW(vfs.translateVFS("file.txt"), Util::PapoException);
}

TEST_F(VFSTest, TranslateVFSSkipsAllowedCheckWhenEnforceAllowedFalse)
{
    Service::VFS vfs = makeStrictVFS();
    EXPECT_NO_THROW(vfs.translateVFS("file.txt", /*enforceAllowed=*/false));
}

TEST_F(VFSTest, TranslateVFSAllowsWhenRootIsAllowed)
{
    Service::VFS vfs = makeStrictVFS({root_});
    EXPECT_NO_THROW(vfs.translateVFS("file.txt"));
}

// ============================================================
// translateFS
// ============================================================

TEST_F(VFSTest, TranslateFSReturnsPathRelativeToRoot)
{
    Service::VFS vfs = makeDangerousVFS();
    fs::path rel = vfs.translateFS(root_ / "sub/file.txt");
    EXPECT_EQ(rel, fs::path("sub/file.txt"));
}

TEST_F(VFSTest, TranslateFSRejectsPathOutsideRoot)
{
    Service::VFS vfs = makeDangerousVFS();
    EXPECT_THROW(vfs.translateFS(sandbox_ / "outside.txt"), Util::PapoException);
}

// ============================================================
// write / readAll round trips
// ============================================================

TEST_F(VFSTest, WriteBytesThenReadAllRoundTrips)
{
    Service::VFS vfs = makeDangerousVFS();
    const std::string payload = "hello virtual file system";
    vfs.write("greeting.txt",
              reinterpret_cast<const std::byte *>(payload.data()),
              payload.size());

    EXPECT_EQ(vfs.readAll("greeting.txt"), payload);
}

TEST_F(VFSTest, WriteBytesEmptyProducesEmptyFile)
{
    Service::VFS vfs = makeDangerousVFS();
    vfs.write("empty.txt", reinterpret_cast<const std::byte *>(""), 0);
    EXPECT_EQ(vfs.readAll("empty.txt"), "");
}

TEST_F(VFSTest, WriteOverwritesExistingFile)
{
    Service::VFS vfs = makeDangerousVFS();
    const std::string first  = "first contents";
    const std::string second = "second";
    vfs.write("f.txt", reinterpret_cast<const std::byte *>(first.data()), first.size());
    vfs.write("f.txt", reinterpret_cast<const std::byte *>(second.data()), second.size());
    EXPECT_EQ(vfs.readAll("f.txt"), second);
}

TEST_F(VFSTest, WriteFromInputStreamRoundTrips)
{
    // Exercises the istream overload: write(path, std::istream&).
    Service::VFS vfs = makeDangerousVFS();
    const std::string payload = "streamed payload contents";
    std::istringstream in(payload);

    vfs.write("streamed.txt", in);

    EXPECT_EQ(vfs.readAll("streamed.txt"), payload);
}

TEST_F(VFSTest, GetWriteHandleIsUsableDirectly)
{
    Service::VFS vfs = makeDangerousVFS();
    {
        std::ofstream out = vfs.getWriteHandle("direct.txt");
        ASSERT_TRUE(out.is_open());
        out << "direct write";
    }
    EXPECT_EQ(vfs.readAll("direct.txt"), "direct write");
}

TEST_F(VFSTest, GetReadHandleReadsExistingFile)
{
    Service::VFS vfs = makeDangerousVFS();
    const std::string payload = "read handle contents";
    vfs.write("rh.txt", reinterpret_cast<const std::byte *>(payload.data()), payload.size());

    std::ifstream in = vfs.getReadHandle("rh.txt");
    ASSERT_TRUE(in.is_open());
    std::string got((std::istreambuf_iterator<char>(in)), {});
    EXPECT_EQ(got, payload);
}

// ============================================================
// copyToVFS
// ============================================================

TEST_F(VFSTest, CopyToVFSCopiesContents)
{
    Service::VFS vfs = makeDangerousVFS();
    fs::path src = sandbox_ / "source.txt";
    const std::string payload = writeRealFile(src, "copy me into the vfs");

    vfs.copyToVFS(src, "copied.txt");

    EXPECT_EQ(vfs.readAll("copied.txt"), payload);
}

TEST_F(VFSTest, CopyToVFSRejectsDisallowedSourceWhenStrict)
{
    Service::VFS vfs = makeStrictVFS(); // no allowed paths
    fs::path src = sandbox_ / "source.txt";
    writeRealFile(src, "data");

    EXPECT_THROW(vfs.copyToVFS(src, "copied.txt"), Util::PapoException);
}

// ============================================================
// exposeToVFS
// ============================================================

TEST_F(VFSTest, ExposeToVFSCreatesSymlinkToSource)
{
    Service::VFS vfs = makeDangerousVFS();
    fs::path src = sandbox_ / "real.txt";
    const std::string payload = writeRealFile(src, "real external contents");

    fs::path virt = vfs.exposeToVFS(src, "exposed.txt");
    EXPECT_EQ(virt, fs::path("exposed.txt"));

    fs::path resolved = vfs.translateVFS("exposed.txt");
    EXPECT_TRUE(fs::is_symlink(resolved));
    EXPECT_EQ(vfs.readAll("exposed.txt"), payload);
}

TEST_F(VFSTest, ExposeToVFSThrowsWhenDestinationExists)
{
    Service::VFS vfs = makeDangerousVFS();
    fs::path src = sandbox_ / "real.txt";
    writeRealFile(src, "data");

    // Occupy the destination first.
    vfs.write("taken.txt", reinterpret_cast<const std::byte *>("x"), 1);

    EXPECT_THROW(vfs.exposeToVFS(src, "taken.txt"), Util::PapoException);
}

TEST_F(VFSTest, ExposeToVFSRejectsDisallowedSourceWhenStrict)
{
    Service::VFS vfs = makeStrictVFS(); // no allowed paths
    fs::path src = sandbox_ / "real.txt";
    writeRealFile(src, "data");

    EXPECT_THROW(vfs.exposeToVFS(src, "exposed.txt"), Util::PapoException);
}

// ============================================================
// allowed-path management
// ============================================================

TEST_F(VFSTest, AddAllowedPathMakesSubdirAllowed)
{
    Service::VFS vfs = makeStrictVFS();
    EXPECT_FALSE(vfs.checkIfPathIsAllowed(sandbox_ / "a/b"));

    vfs.addAllowedPath(sandbox_);
    EXPECT_TRUE(vfs.checkIfPathIsAllowed(sandbox_ / "a/b"));
}

TEST_F(VFSTest, CheckIfPathIsAllowedFalseForUnrelatedPath)
{
    Service::VFS vfs = makeStrictVFS({sandbox_ / "allowed"});
    EXPECT_FALSE(vfs.checkIfPathIsAllowed(sandbox_ / "other"));
}

TEST_F(VFSTest, RemoveAllowedPathRevokesAccess)
{
    Service::VFS vfs = makeStrictVFS({sandbox_});
    ASSERT_TRUE(vfs.checkIfPathIsAllowed(sandbox_ / "x"));

    vfs.removeAllowedPath(sandbox_);
    EXPECT_FALSE(vfs.checkIfPathIsAllowed(sandbox_ / "x"));
}

TEST_F(VFSTest, PopAllowedPathRemovesLast)
{
    Service::VFS vfs = makeStrictVFS({sandbox_ / "first", sandbox_ / "second"});
    vfs.popAllowedPath();
    EXPECT_TRUE(vfs.checkIfPathIsAllowed(sandbox_ / "first/x"));
    EXPECT_FALSE(vfs.checkIfPathIsAllowed(sandbox_ / "second/x"));
}

// ============================================================
// checkIfSubdir (static)
// ============================================================

TEST(VFSCheckIfSubdir, DirectChildIsSubdir)
{
    EXPECT_TRUE(Service::VFS::checkIfSubdir("/a/b", "/a/b/c"));
}

TEST(VFSCheckIfSubdir, DeepDescendantIsSubdir)
{
    EXPECT_TRUE(Service::VFS::checkIfSubdir("/a/b", "/a/b/c/d/e"));
}

TEST(VFSCheckIfSubdir, IdenticalPathIsSubdir)
{
    EXPECT_TRUE(Service::VFS::checkIfSubdir("/a/b", "/a/b"));
}

TEST(VFSCheckIfSubdir, SiblingIsNotSubdir)
{
    EXPECT_FALSE(Service::VFS::checkIfSubdir("/a/b", "/a/c"));
}

TEST(VFSCheckIfSubdir, ParentIsNotSubdirOfChild)
{
    EXPECT_FALSE(Service::VFS::checkIfSubdir("/a/b/c", "/a/b"));
}

TEST(VFSCheckIfSubdir, PrefixButNotPathBoundaryIsNotSubdir)
{
    // "/a/bc" is not under "/a/b" despite the string prefix.
    EXPECT_FALSE(Service::VFS::checkIfSubdir("/a/b", "/a/bc"));
}
