#pragma once

#include "services/render/model.hpp"
namespace Runtime {

class RenderService {
public:
struct ShaderCTX {}; 
  void renderModel(Service::Model *model, ShaderCTX shaderCTX);

private:
};


} // namespace Runtime