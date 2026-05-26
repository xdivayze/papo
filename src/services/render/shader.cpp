#include "services/render/shader.hpp"
#include "glad/gl.h"
#include "glm/gtc/type_ptr.hpp"
#include "utils/exception.hpp"

namespace Services {

unsigned int ShaderManager::getProgram(ShaderManager::FeatureMask features) {
  if (programCache_.contains(features))
    return programCache_[features];

  programCache_[features] = createProgram(features);
  return programCache_[features];
}

int ShaderManager::getUniformLocation(unsigned int programID,
                                      std::string_view name) {
  if (auto it = uniformCache_.find(programID); it != uniformCache_.end()) {
    for (auto &[uName, uLoc] : it->second) {
      if (name == uName)
        return uLoc;
    }
  }

  std::string nameStr = std::string(name);
  int loc = glGetUniformLocation(programID, nameStr.c_str());
  uniformCache_[programID].emplace_back(nameStr, loc);
  return loc;
}

int ShaderManager::getUniformLocation(std::string_view name) {
  return getUniformLocation(currentProgramID_, name);
}

void ShaderManager::setUniform(std::string_view name, bool val) {
  glProgramUniform1ui(currentProgramID_, getUniformLocation(name), val);
}
void ShaderManager::setUniform(std::string_view name, int val) {
  glProgramUniform1i(currentProgramID_, getUniformLocation(name), val);
}
void ShaderManager::setUniform(std::string_view name, float val) {
  glProgramUniform1f(currentProgramID_, getUniformLocation(name), val);
}
void ShaderManager::setUniform(std::string_view name, glm::vec3 val) {
  glProgramUniform3fv(currentProgramID_, getUniformLocation(name), 1,
                      glm::value_ptr(val));
}
void ShaderManager::setUniform(std::string_view name, glm::mat4 val) {
  glProgramUniformMatrix4fv(currentProgramID_, getUniformLocation(name), 1,
                            GL_FALSE, glm::value_ptr(val));
}

void ShaderManager::setUniform(int uniformLoc, bool val) {
  glProgramUniform1ui(currentProgramID_, uniformLoc, val);
}
void ShaderManager::setUniform(int uniformLoc, int val) {
  glProgramUniform1i(currentProgramID_, uniformLoc, val);
}
void ShaderManager::setUniform(int uniformLoc, float val) {
  glProgramUniform1f(currentProgramID_, uniformLoc, val);
}
void ShaderManager::setUniform(int uniformLoc, glm::vec3 val) {
  glProgramUniform3fv(currentProgramID_, uniformLoc, 1, glm::value_ptr(val));
}
void ShaderManager::setUniform(int uniformLoc, glm::mat4 val) {
  glProgramUniformMatrix4fv(currentProgramID_, uniformLoc, 1, GL_FALSE,
                            glm::value_ptr(val));
}

void ShaderManager::setUniform(unsigned int programID, std::string_view name,
                               bool val) {
  glProgramUniform1ui(programID, getUniformLocation(name), val);
}
void ShaderManager::setUniform(unsigned int programID, std::string_view name,
                               int val) {
  glProgramUniform1i(programID, getUniformLocation(name), val);
}
void ShaderManager::setUniform(unsigned int programID, std::string_view name,
                               float val) {
  glProgramUniform1f(programID, getUniformLocation(name), val);
}
void ShaderManager::setUniform(unsigned int programID, std::string_view name,
                               glm::vec3 val) {
  glProgramUniform3fv(programID, getUniformLocation(name), 1,
                      glm::value_ptr(val));
}
void ShaderManager::setUniform(unsigned int programID, std::string_view name,
                               glm::mat4 val) {
  glProgramUniformMatrix4fv(programID, getUniformLocation(name), 1, GL_FALSE,
                            glm::value_ptr(val));
}

void ShaderManager::setUniform(unsigned int programID, int uniformLoc,
                               bool val) {
  glProgramUniform1ui(programID, uniformLoc, val);
}
void ShaderManager::setUniform(unsigned int programID, int uniformLoc,
                               int val) {
  glProgramUniform1i(programID, uniformLoc, val);
}
void ShaderManager::setUniform(unsigned int programID, int uniformLoc,
                               float val) {
  glProgramUniform1f(programID, uniformLoc, val);
}
void ShaderManager::setUniform(unsigned int programID, int uniformLoc,
                               glm::vec3 val) {
  glProgramUniform3fv(programID, uniformLoc, 1, glm::value_ptr(val));
}
void ShaderManager::setUniform(unsigned int programID, int uniformLoc,
                               glm::mat4 val) {
  glProgramUniformMatrix4fv(programID, uniformLoc, 1, GL_FALSE,
                            glm::value_ptr(val));
}

ShaderManager::ShaderManager(std::string &&vertexShader,
                             std::string &&fragmentShader)
    : vertexShaderBase_(std::move(vertexShader)),
      fragmentShaderBase_(std::move(fragmentShader)) {
  currentProgramID_ = getProgram(Features::NORMAL_MAP);
}

// TODO ctor with the filesystem read

static int checkIfProgramSuccessful(unsigned int program) {
  int success;
  glGetProgramiv(program, GL_LINK_STATUS, &success);
  if (!success) {
    return -1;
  }
  return 0;
}

unsigned int ShaderManager::createProgram(FeatureMask features) const {
  std::vector<std::string_view> defines = featureMapper(features);

  unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
  compileAndLinkShader(vertexShader, vertexShaderBase_, defines);

  unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
  compileAndLinkShader(fragmentShader, fragmentShaderBase_, defines);

  unsigned int program = glCreateProgram();
  glAttachShader(program, vertexShader);
  glAttachShader(program, fragmentShader);

  glLinkProgram(program);

  if (checkIfProgramSuccessful(program)) {
    throw Util::PapoException(TAG, "createProgram::PROGRAM LINK FAILED");
  }

  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);

  return program;
}

static int checkIfShaderSuccessful(unsigned int shader) {
  int success;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    return -1;
  }
  return 0;
}

void ShaderManager::compileAndLinkShader(
    unsigned int shader, const std::string &shaderBody,
    const std::vector<std::string_view> &defines) const {
  std::string src = "#version 450 core\n";
  for (auto d : defines) {
    src += "#define " + std::string(d) + "\n";
  }
  src += shaderBody;

  const char *src_cstr = shaderBody.c_str();
  glShaderSource(shader, 1, &src_cstr, NULL);
  glCompileShader(shader);

  if (checkIfShaderSuccessful(shader)) {
    throw Util::PapoException(
        TAG, "compileAndLinkShader::shader compilation failed");
  }
}

} // namespace Services