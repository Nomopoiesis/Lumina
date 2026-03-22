#include "path_registry.hpp"

#include "lumina_assert.hpp"

namespace lumina::common {

PathRegistry *PathRegistry::instance_ = nullptr;

auto PathRegistry::Initialize(std::filesystem::path runtime_root) -> void {
  ASSERT(instance_ == nullptr, "PathRegistry already initialized");
  instance_ = new PathRegistry(runtime_root);
}

auto PathRegistry::Instance() -> PathRegistry & {
  ASSERT(instance_ != nullptr, "PathRegistry not initialized");
  return *instance_;
}

auto PathRegistry::Shutdown() -> void {
  delete instance_;
  instance_ = nullptr;
}

} // namespace lumina::common
