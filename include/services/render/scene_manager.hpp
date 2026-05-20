#include "services/render/renderable_object.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace Runtime {

class SceneManager {
public:
};

class Scene {
public:
  virtual void renderScene();

private:
  virtual void addObject(RenderableObject &&model);
  virtual void removeObject(uint32_t modelId);
};

// TODO add physics engine
class StreamingScene : public Scene { // churning edge
public:
  struct LayerDescriptor {
    std::size_t layerEndDistance;
    std::size_t layerHysteresis;
  };

  struct LayerDescriptorList {
    LayerDescriptor stableDescriptor;
    LayerDescriptor hotTransientDescriptor;
    LayerDescriptor warmTransientDescriptor;
    LayerDescriptor RAMLoadedDescriptor;
  };

  void renderScene() override;

  // add or remove model from the appropriate vector/asset file depending on the
  // coordinates
  void addObject(RenderableObject &&model) override;
  void removeObject(uint32_t modelId) override;

  StreamingScene(LayerDescriptorList &&layerDescriptorList);
  ~StreamingScene();

private:
  LayerDescriptorList layerDescriptorList_;

  // O(1) swap and pop or O(1) push_back
  std::vector<RenderableObject> stableRendered_;

  std::vector<RenderableObject> transientRendered_;
  std::vector<RenderableObject> transientLoaded_;
  std::vector<RenderableObject> ramLoaded_;
  //

  const std::string
      assetListFile_; // filesystem stored scene graph with model descriptors
};

} // namespace Runtime