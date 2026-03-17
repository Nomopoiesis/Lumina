#pragma once

#include "core/resource_manager.hpp"

#include "shaders/shader_interface.hpp"

namespace lumina::renderer {

struct MaterialTemplateCreateError {
  std::string message;
};

struct MaterialTemplateDescription {
  ShaderInterface *shader_interface;
  ShaderLayout vertex_layout;
  ShaderLayout fragment_layout;
  std::string vertex_shader_bin_path;
  std::string fragment_shader_bin_path;
  size_t max_instances;
};

class MaterialTemplate {
public: // static methods
  static auto Create(const MaterialTemplateDescription &description)
      -> std::expected<MaterialTemplate, MaterialTemplateCreateError>;

public: // instance methods
  MaterialTemplate() noexcept = default;
  MaterialTemplate(const MaterialTemplate &) noexcept = default;
  MaterialTemplate(MaterialTemplate &&) noexcept = default;
  auto operator=(const MaterialTemplate &) noexcept
      -> MaterialTemplate & = default;
  auto operator=(MaterialTemplate &&) noexcept -> MaterialTemplate & = default;
  ~MaterialTemplate() noexcept = default;

  [[nodiscard]] auto GetShaderInterface() const -> ShaderInterface * {
    return shader_interface;
  }

  [[nodiscard]] auto GetVertexShaderBinPath() const -> const std::string & {
    return vertex_shader_bin_path;
  }

  [[nodiscard]] auto GetFragmentShaderBinPath() const -> const std::string & {
    return fragment_shader_bin_path;
  }

private: // member variables
  ShaderInterface *shader_interface = nullptr;
  std::string vertex_shader_bin_path;
  std::string fragment_shader_bin_path;
};

} // namespace lumina::renderer