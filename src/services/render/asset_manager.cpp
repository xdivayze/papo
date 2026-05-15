#include "services/render/asset_manager.hpp"

#include "services/async/thread_pool_manager.hpp"
#include "services/render/mesh.hpp"
#include "utils/exception.hpp"

#include <mutex>
#include <new>
#include <utility>

namespace {
using MeshT = Service::Mesh<VertexTypes::Vertex1P1N1UV>;

// Byte size of each leased per-model scratch stack. Holds one model's
// transient vertex/index buffers until Load(true) reclaims them. Tunable.
constexpr std::uint32_t MODEL_SCRATCH_STACK_BYTES = 8u << 20; // 8 MiB

// Capacity for the long-lived object pools. Stacks are transient (returned
// on a clean GPU upload) so the StackAllocater pool is sized by `nstacks`,
// but Models/Meshes/Materials live as long as the cached model, so they
// need a capacity independent of the concurrency bound. Tunable.
constexpr std::size_t MAX_MODELS = 256;
} // namespace

namespace Service {

AssetManager::AssetManager(Memory::PoolManager &poolManager,
                           MemoryManager &memoryManager, size_t nstacks)
    : poolManager_(poolManager), memoryManager_(memoryManager) {
  modelData_ = nullptr;     // unused for now
  modelDataStackSize_ = 0;  // unused for now

  poolManager_.registerPool<Model>(MAX_MODELS);
  poolManager_.registerPool<MeshT>(MAX_MODELS * Model::INLINE_MESH_CAP);
  poolManager_.registerPool<Material>(MAX_MODELS * Model::INLINE_MATERIAL_CAP);
  poolManager_.registerPool<StackAllocater>(nstacks);

  freeStacks_.reserve(nstacks);
  for (size_t i = 0; i < nstacks; i++) {
    StackAllocater *slot = poolManager_.acquireFromPool<StackAllocater>();
    if (!slot)
      throw Util::PapoException(TAG, "StackAllocater pool exhausted during "
                                     "AssetManager construction");
    // Constructed once for the AssetManager's lifetime: leasing/returning
    // never mallocs => zero heap allocation per load in steady state.
    freeStacks_.push_back(new (slot) StackAllocater(MODEL_SCRATCH_STACK_BYTES));
  }
}


AssetManager::ModelHandle<Model>
AssetManager::modelFromFilePath(std::string_view filepath, bool deferGL) {
  std::string key(filepath);

  std::unique_lock<std::mutex> lk(leaseMutex_);

  auto cached = cache_.find(key);
  if (cached != cache_.end())
    return cached->second; // dedupe: no new lease, no id consumed

  // Block until a per-model scratch stack is available.
  leaseCv_.wait(lk, [this] { return !freeStacks_.empty(); });

  StackAllocater *stack = freeStacks_.back();
  freeStacks_.pop_back();
  std::uint32_t id = idCounter_++;

  // The assimp parse + GL buffer generation in the Model ctor is heavy;
  // don't hold the lease lock across it (would serialize concurrent loads).
  lk.unlock();

  Model *slot = poolManager_.acquireFromPool<Model>();
  if (!slot) {
    lk.lock();
    freeStacks_.push_back(stack);
    leaseCv_.notify_one();
    lk.unlock();
    throw Util::PapoException(TAG, "Model pool exhausted");
  }

  Model *model;
  try {
    model = new (slot) Model(poolManager_, stack, key, id, deferGL);
  } catch (...) {
    lk.lock();
    freeStacks_.push_back(stack);
    leaseCv_.notify_one();
    lk.unlock();
    poolManager_.releaseToPool(slot);
    throw; // e.g. Util::PapoException on a bad/missing file
  }

  ModelHandle<Model> handle{id, model};

  lk.lock();
  allocatorLeaseMap[id] = stack;
  cache_[key] = handle;
  lk.unlock();

  return handle;
}

std::future<AssetManager::ModelHandle<Model>>
AssetManager::modelFromFilePathAsync(async::ThreadPoolManager &pool,
                                     std::string_view filepath) {
  // Copy into an owning string: the worker may run long after `filepath`'s
  // backing storage is gone (string_view does not own).
  std::string key(filepath);
  auto job = [this, key = std::move(key)]() -> ModelHandle<Model> {
    // deferGL=true: the worker has no GL context, so skip buffer generation.
    // Buffers are created on the render thread (loadModelToGPU / explicit
    // Model::initializeMeshBuffers).
    return modelFromFilePath(key, /*deferGL=*/true); // thread-safe
  };
  return pool.enqueueTask(
      async::Task<decltype(job)>{std::move(job), async::REGULAR});
}

void AssetManager::loadModelToGPU(ModelHandle<Model> model,
                                  bool cleanLastCPUData) {
  // Private Model::Load (AssetManager is a friend): uploads each mesh to the
  // GPU and, when cleanLastCPUData, reclaims the transient scratch and nulls
  // the model's stack_.
  model.ptr->Load(cleanLastCPUData);

  if (!cleanLastCPUData)
    return; // caller keeps CPU data => lease intentionally retained

  std::lock_guard<std::mutex> lk(leaseMutex_);
  auto leased = allocatorLeaseMap.find(model.id);
  if (leased == allocatorLeaseMap.end())
    return; // already returned, or a cache reuse that never held a lease

  StackAllocater *stack = leased->second;
  stack->clear(); // defensive: hand back an empty stack
  allocatorLeaseMap.erase(leased);
  freeStacks_.push_back(stack);
  leaseCv_.notify_one();
}

AssetManager::~AssetManager() {
  // NOTE: ~Model -> ~Mesh -> glDelete* — a current GL context must still be
  // live at AssetManager teardown (same GL-lifecycle assumption as Mesh).
  for (auto &entry : cache_) {
    Model *m = entry.second.ptr;
    m->~Model();
    poolManager_.releaseToPool(m);
  }
  cache_.clear();

  // Every stack lives either in the free list or still leased out.
  for (StackAllocater *s : freeStacks_) {
    s->~StackAllocater();
    poolManager_.releaseToPool(s);
  }
  for (auto &entry : allocatorLeaseMap) {
    StackAllocater *s = entry.second;
    s->~StackAllocater();
    poolManager_.releaseToPool(s);
  }
  freeStacks_.clear();
  allocatorLeaseMap.clear();

  poolManager_.removePoolAllocater<Model>();
  poolManager_.removePoolAllocater<MeshT>();
  poolManager_.removePoolAllocater<Material>();
  poolManager_.removePoolAllocater<StackAllocater>();
}

} // namespace Service
