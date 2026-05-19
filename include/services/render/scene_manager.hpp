#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float3.hpp"
#include "services/render/asset_manager.hpp"
#include "services/render/model.hpp"
#include <cstdint>
#include <string>
#include <vector>
namespace Service {

class SceneManager {
public:
};

class Scene {
public:
  struct SceneModel {
    AssetManager::ModelHandle<Model> model;
    glm::mat4 transform;
  };

  struct SceneModelNotBaked {

    AssetManager::ModelHandle<Model> model;
    glm::vec3 coordinates;
    glm::vec3 rotation;
    glm::vec3 scalingFactor;
  };

  virtual void renderScene();

private:
  const std::string assetListFile_; // json list of model filepaths and
                                    // coordinates in the whole game,

  virtual void addModel(SceneModelNotBaked &&model);
  virtual void removeModel(uint32_t modelId);
};

class StreamingScene : public Scene { // churning edge
public:
  void renderScene() override;

  // add or remove model from the appropriate vector/asset file depending on the
  // coordinates
  void addModel(SceneModelNotBaked &&model) override;
  void removeModel(uint32_t modelId) override;

private:
  // O(1) swap and pop or O(1) push_back
  std::vector<SceneModel> stableRendered_;

  std::vector<SceneModel> transientRendered_;
  std::vector<SceneModel> transientLoaded_;
};

} // namespace Service