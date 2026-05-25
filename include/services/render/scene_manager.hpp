#include "services/render/camera.hpp"
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
  virtual void renderScene() const;
  virtual ICamera &camera() const { return camera_; }

  virtual void frameStartCallback();

  Scene(ICamera &camera);
  ~Scene() = default;

private:
  virtual void addObject(RenderableObject &&model);
  virtual void removeObject(uint32_t modelId);

  ICamera &camera_;
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

    enum DescriptorNames { STABLE, HOT, WARM, RAM, COLD };

    inline constexpr DescriptorNames
    distanceMatcher(std::size_t distance) const noexcept {
      if (distance <= stableDescriptor.layerEndDistance)
        return DescriptorNames::STABLE;
      if (distance <= hotTransientDescriptor.layerEndDistance)
        return DescriptorNames::HOT;
      if (distance <= warmTransientDescriptor.layerEndDistance)
        return DescriptorNames::WARM;
      if (distance <= RAMLoadedDescriptor.layerEndDistance)
        return DescriptorNames::RAM;
      return DescriptorNames::COLD;
    }
  };

  void renderScene() const override;

  void frameStartCallback() override;

  // add or remove model from the appropriate vector/asset file depending on the
  // coordinates
  void addObject(RenderableObject &&model) override;
  void removeObject(uint32_t modelId) override;

  StreamingScene(LayerDescriptorList &&layerDescriptorList, ICamera &camera);
  ~StreamingScene() = default;

private:
  std::vector<RenderableObject> *
  distanceToRenderVector(std::size_t distance) noexcept;

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