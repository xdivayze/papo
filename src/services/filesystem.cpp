#include "services/filesystem.hpp"
#include "utils/exception.hpp"
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace fs = std::filesystem;
namespace Service {

std::ofstream VFS::getWriteHandle(const fs::path &virtualFilepath) const {
  return std::ofstream(translateVFS(virtualFilepath));
}

std::ifstream
VFS::getReadHandle(const std::filesystem::path &virtualFilepath) const {
  return std::ifstream(translateVFS(virtualFilepath));
}

void *VFS::vmmap(const std::filesystem::path &virtualFilepath, int prot,
                 int flags) const {
  fs::path p = translateVFS(virtualFilepath);

  int oflag;
  if ((prot & PROT_READ) && (prot & PROT_WRITE))
    oflag = O_RDWR;
  else if (prot & PROT_READ)
    oflag = O_RDONLY;
  else if (prot & PROT_WRITE)
    oflag = O_WRONLY;

  int fd = ::open(p.generic_string().c_str(), oflag);
  if (fd == -1)
    throw Util::PapoException(
        TAG, "vmmap::open::Error while trying to open file descriptor");

  struct stat st{};
  if (::fstat(fd, &st) == -1)
    throw Util::PapoException(TAG,
                              "vmmap::fstat::Error while getting fd stats");

  std::size_t size = static_cast<std::size_t>(st.st_size);

  void *fp = size ? mmap(nullptr, size, prot, flags, fd, 0) : nullptr;
  ::close(fd);

  if (size != 0 && fp == MAP_FAILED)
    throw Util::PapoException(TAG, "vmmap::Map failed");

  return fp;
}

std::string VFS::readAll(const std::filesystem::path &virtualFilepath) const {
  auto f = getReadHandle(virtualFilepath);
  std::ostringstream sstream;
  sstream << f.rdbuf();
  return sstream.str();
}

// creates unfortunate copy
void VFS::write(const fs::path &virtualFilepath, const std::byte *data,
                size_t len) {
  auto stream = getWriteHandle(virtualFilepath);
  stream.write(reinterpret_cast<const char *>(data), len);
}
void VFS::write(const fs::path &virtualFilepath, std::streambuf *buf) {
  auto stream = getWriteHandle(virtualFilepath);
  stream << buf;
}

/*
check if source path is already in VFS(return VFS path)
check if destination path is already in use (exists in VFS)
link and return (very dangerous btw)
*/
fs::path VFS::exposeToVFS(const std::filesystem::path &absPath,
                          const std::filesystem::path &virtualFilepath) {
  if (!checkIfPathIsAllowed(absPath) && !allowDangerous_)
    throw Util::PapoException(TAG, "exposeToVFS::dangerous functions are not "
                                   "allowed if the path is not allowed");

  fs::path destAbsolute = translateVFS(virtualFilepath);
  try {
    return translateFS(absPath);
  } catch (Util::PapoException e) {
  }
  if (fs::exists(destAbsolute))
    throw Util::PapoException(
        TAG, "exposeToVFS::destination file path already exists");

  fs::create_symlink(absPath, destAbsolute);

  return virtualFilepath;
}

std::filesystem::path copyToVFS(const std::filesystem::path &absPath,
                                const std::filesystem::path &virtualFilepath) {
                                  
                                }

fs::path VFS::translateFS(const fs::path &filepath, bool enforceAllowed) const {
  fs::path abs = fs::weakly_canonical(filepath);

  if ((enforceAllowed && allowDangerous_) && !checkIfPathIsAllowed(abs))
    throw Util::PapoException(TAG, "translateFS::illegal filepath");

  if (!checkIfSubdir(root_, abs))
    throw Util::PapoException(TAG, "translateFS::filepath not in VFS");

  return fs::relative(filepath, root_);
}

fs::path VFS::translateVFS(const fs::path &virtualFilepath,
                           bool enforceAllowed) const {

  if (virtualFilepath.is_absolute())
    throw Util::PapoException(
        TAG, "translateVFS::virtual file path must be relative");

  fs::path candidate = fs::weakly_canonical(root_ / virtualFilepath);

  if (!checkIfSubdir(root_, candidate))
    throw Util::PapoException(TAG,
                              "translateVFS::virtual filepath outside VFS");

  if ((enforceAllowed && !allowDangerous_) && !checkIfPathIsAllowed(candidate))
    throw Util::PapoException(TAG, "translateVFS::illegal filepath");

  return std::move(candidate);
}
} // namespace Service
