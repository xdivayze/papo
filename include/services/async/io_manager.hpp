#pragma once

#include <cstddef>
#include <string>
namespace Service {
class IOManager {
public:
  struct IOHandleDataReturn {
    std::byte *data;
    std::size_t dataLen;
  };

  struct IOHandle {
    static IOHandleDataReturn Data() {
      // TODO

      return {nullptr, 0};
    };

    static IOHandleDataReturn DataBuffered(std::size_t readSize) {

      // TODO
      return {nullptr, 0};
    };

    std::string filepath;
  };

  IOHandle GetHandleFromFilepath(std::string filepath) {
    // TODO initialize user buffer for file reading
    return {.filepath = std::move(filepath.data())};
  }

};
} // namespace Service