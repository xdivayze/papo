#pragma once

#include "collections/robin_hood.hpp"
#include "glad/gl.h"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float3.hpp"
#include "services/render/asset_manager.hpp"
#include <cstdint>
#include <string_view>
#include <vector>
namespace Services {

class ShaderManager {
public:
  static constexpr const char *TAG = "Shader Manager";

  enum Features : uint8_t {
    NORMAL_MAP = 1 << 0,
  };

  using FeatureMask = uint8_t;

  inline static constexpr std::pair<Features, std::string_view>
      kFeatureDefines[] = {
          {NORMAL_MAP, "NORMAL_MAP"},
  };

  // return a vector of string views depending on the feature mask
  inline static constexpr std::vector<std::string_view>
  featureMapper(FeatureMask mask) {
    std::vector<std::string_view> out;
    for (auto &[feat, name] : kFeatureDefines) {
      if (feat & mask)
        out.push_back(name);
    }

    return out;
  }

  inline void useProgram(unsigned int program) {
    if (currentProgramID_ == program)
      return;
    glUseProgram(program);
    currentProgramID_ = program;
    return;
  }

  // cache manager
  unsigned int getProgram(FeatureMask features);

  int getUniformLocation(unsigned int programID, std::string_view nameView);

  // all of the following require the program to be set first
  int getUniformLocation(std::string_view nameView);

  //-do cache (and) string query
  void setUniform(std::string_view name, bool val);
  void setUniform(std::string_view name, int val);
  void setUniform(std::string_view name, float val);
  void setUniform(std::string_view name, glm::vec3 val);
  void setUniform(std::string_view name, glm::mat4 val);
  //-

  //-bypasses the cache and string query
  void setUniform(int uniformLoc, bool val);
  void setUniform(int uniformLoc, int val);
  void setUniform(int uniformLoc, float val);
  void setUniform(int uniformLoc, glm::vec3 val);
  void setUniform(int uniformLoc, glm::mat4 val);
  //-

  //- does cache (and) string query, transitions briefly to the program id to
  // set the uniform( program id is reverted after)
  void setUniform(unsigned int programID, std::string_view name, bool val);
  void setUniform(unsigned int programID, std::string_view name, int val);
  void setUniform(unsigned int programID, std::string_view name, float val);
  void setUniform(unsigned int programID, std::string_view name, glm::vec3 val);
  void setUniform(unsigned int programID, std::string_view name, glm::mat4 val);
  //-

  //-bypasses the cache and string, query transitions briefly to the program id
  //-to set the uniform( program id is reverted after)
  void setUniform(unsigned int programID, int uniformLoc, bool val);
  void setUniform(unsigned int programID, int uniformLoc, int val);
  void setUniform(unsigned int programID, int uniformLoc, float val);
  void setUniform(unsigned int programID, int uniformLoc, glm::vec3 val);
  void setUniform(unsigned int programID, int uniformLoc, glm::mat4 val);
  //-
  //

  explicit ShaderManager(std::string_view vertexShaderName,
                         std::string_view fragmentShaderName,
                         Service::AssetManager &assetManager);

  explicit ShaderManager(std::string &&vertexShader,
                         std::string &&fragmentShader);
  ~ShaderManager() = default;

private:
  // create and return program
  unsigned int createProgram(FeatureMask features) const;

  void compileAndLinkShader(unsigned int shader, const std::string &shaderBody,
                            const std::vector<std::string_view> &defines) const;

  robin_hood::unordered_map<FeatureMask, unsigned int> programCache_;

  //? maybe switch to another hashmap instead of the vector
  //-1 on inactive uniform
  robin_hood::unordered_map<unsigned int,
                            std::vector<std::pair<std::string, int>>>
      uniformCache_;

  const std::string vertexShaderBase_;
  const std::string fragmentShaderBase_;

  unsigned int currentProgramID_;
};
} // namespace Services