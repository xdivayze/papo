#pragma once

#include <filesystem>
#include <fstream>
#include <streambuf>
#include <string>
#include <vector>
namespace Service {
class VFS {
public:
  static constexpr const char *TAG = "Virtual File System";

  // TODO custom buffer implementation with O_DIRECT

  // converts to canonical path and checks if root
  static bool checkIfSubdir(const std::filesystem::path &root,
                            const std::filesystem::path &candidate) noexcept {
    auto root_can = std::filesystem::weakly_canonical(root);
    auto candidate_can = std::filesystem::weakly_canonical(candidate);
    auto root_it = root_can.begin();
    auto cand_it = candidate_can.begin();
    for (; root_it != root.end(); ++root_it, ++cand_it) {
      if (cand_it == candidate.end() || *cand_it != *root_it)
        return false;
    }
    return true;
  }

  std::ofstream
  getWriteHandle(const std::filesystem::path &virtualFilepath) const;
  std::ifstream
  getReadHandle(const std::filesystem::path &virtualFilepath) const;

  // nullptr on size 0 read
  void *vmmap(const std::filesystem::path &virtualFilepath, int prot,
              int flags) const;

  std::string readAll(const std::filesystem::path &virtualFilepath) const;

  void write(const std::filesystem::path &virtualFilepath,
             const std::byte *data, size_t len);
  void write(const std::filesystem::path &virtualFilepath, std::streambuf *buf);

  /*
   returns the virtual path if the absPath is already in VFS. Otherwise,
   creates symlink to absPath in VFS and returns. Throws if virtualFilePath
   already exists. Throws if allowDangerous_ is false and absPath is not an
   allowed directory.
  */
  std::filesystem::path
  exposeToVFS(const std::filesystem::path &absPath,
              const std::filesystem::path &virtualFilepath);
  std::filesystem::path copyToVFS(const std::filesystem::path &absPath,
                                  const std::filesystem::path &virtualFilepath);

  /*
    resolve VFS path to absolute filepath, optionally
    (enforceAllowed && !allowDangerous_)
    check if resolved path is allowed
  */
  std::filesystem::path
  translateVFS(const std::filesystem::path &virtualFilepath,
               bool enforceAllowed = true) const;

  /*
    optionally (enforceAllowed && !allowDangerous_) check if the filepath is in
    allowed directories, then resolve the relative path
  */
  std::filesystem::path translateFS(const std::filesystem::path &filepath,
                                    bool enforceAllowed = true) const;

  // push back
  void addAllowedPath(const std::filesystem::path path);

  // pop and swap
  void removeAllowedPath(const std::filesystem::path &path);

  inline void popAllowedPath() { allowedPaths_.pop_back(); }

  // checks if path is a subdirectory of any directories in allowedPaths.
  // Does not need the path to be canonical
  inline bool
  checkIfPathIsAllowed(const std::filesystem::path &candidate) const noexcept {
    for (auto it = allowedPaths_.begin(); it != allowedPaths_.end(); ++it) {
      if (checkIfSubdir(*it, candidate))
        return true;
    }
    return false;
  }

  VFS(std::vector<std::filesystem::path> allowedPaths,
      bool allowDangerous = false);

private:
  bool allowDangerous_ = false;

  // allowed path vector (subdirectories automatically allowed)
  std::vector<std::filesystem::path> allowedPaths_;

  std::filesystem::path root_;
};
} // namespace Service