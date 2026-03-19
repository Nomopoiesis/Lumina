#define CPPGEN_IMPLEMENTATION
#include <cppgen/cppgen.hpp>
#include <spirv_reflect.h>

#include "lumina_assert.hpp"
#include "lumina_types.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <print>
#include <string>
#include <vector>

// --- Type mapping ---

enum class LuminaType : u8 {
  Bool,
  Int8,
  Uint8,
  Int16,
  Uint16,
  Int32,
  Uint32,
  Int64,
  Uint64,
  Float,
  Double,
  Vec2,
  Vec3,
  Vec4,
  Mat2,
  Mat3,
  Mat4,
  Count_,
};

static auto TypeName(LuminaType type) -> std::string {
  switch (type) {
    case LuminaType::Bool:
      return "bool";
    case LuminaType::Int8:
      return "int8_t";
    case LuminaType::Uint8:
      return "uint8_t";
    case LuminaType::Int16:
      return "int16_t";
    case LuminaType::Uint16:
      return "uint16_t";
    case LuminaType::Int32:
      return "int32_t";
    case LuminaType::Uint32:
      return "uint32_t";
    case LuminaType::Int64:
      return "int64_t";
    case LuminaType::Uint64:
      return "uint64_t";
    case LuminaType::Float:
      return "float";
    case LuminaType::Double:
      return "double";
    case LuminaType::Vec2:
      return "lumina::math::Vec2";
    case LuminaType::Vec3:
      return "lumina::math::Vec3";
    case LuminaType::Vec4:
      return "lumina::math::Vec4";
    case LuminaType::Mat2:
      return "lumina::math::Mat2";
    case LuminaType::Mat3:
      return "lumina::math::Mat3";
    case LuminaType::Mat4:
      return "lumina::math::Mat4";
    default:
      ASSERT(false, "Unknown type");
      return "unknown";
  }
}

static auto BindingTypeString(SpvReflectDescriptorType type) -> std::string {
  switch (type) {
    case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:
      return "Sampler";
    case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
      return "CombinedImageSampler";
    case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
      return "SampledImage";
    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:
      return "StorageImage";
    case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
      return "UniformTexelBuffer";
    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
      return "StorageTexelBuffer";
    case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
      return "UniformBuffer";
    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
      return "StorageBuffer";
    default:
      ASSERT(false, "Unknown descriptor type");
      return "Unknown";
  }
}

static auto DecideType(const SpvReflectTypeDescription *td) -> LuminaType {
  switch (td->op) {
    case SpvOpTypeBool:
      return LuminaType::Bool;

    case SpvOpTypeFloat:
      return td->traits.numeric.scalar.width == 64 ? LuminaType::Double
                                                   : LuminaType::Float;

    case SpvOpTypeInt:
      if (td->traits.numeric.scalar.signedness) {
        switch (td->traits.numeric.scalar.width) {
          case 8:
            return LuminaType::Int8;
          case 16:
            return LuminaType::Int16;
          case 64:
            return LuminaType::Int64;
          default:
            return LuminaType::Int32;
        }
      } else {
        switch (td->traits.numeric.scalar.width) {
          case 8:
            return LuminaType::Uint8;
          case 16:
            return LuminaType::Uint16;
          case 64:
            return LuminaType::Uint64;
          default:
            return LuminaType::Uint32;
        }
      }

    case SpvOpTypeVector:
      switch (td->traits.numeric.vector.component_count) {
        case 2:
          return LuminaType::Vec2;
        case 3:
          return LuminaType::Vec3;
        case 4:
          return LuminaType::Vec4;
        default:
          ASSERT(false, "Unknown vector size");
          return LuminaType::Count_;
      }

    case SpvOpTypeMatrix:
      switch (td->traits.numeric.matrix.column_count) {
        case 2:
          return LuminaType::Mat2;
        case 3:
          return LuminaType::Mat3;
        case 4:
          return LuminaType::Mat4;
        default:
          ASSERT(false, "Unknown matrix size");
          return LuminaType::Count_;
      }

    default:
      ASSERT(false, "Unhandled SPIR-V type op");
      return LuminaType::Count_;
  }
}

static auto StageEnumString(const std::string &stage) -> std::string {
  if (stage == "vert")
    return "lumina::renderer::ShaderStage::Vertex";
  if (stage == "frag")
    return "lumina::renderer::ShaderStage::Fragment";
  if (stage == "global")
    return "lumina::renderer::ShaderStage::Global";
  ASSERT(false, "Unknown shader stage");
  return "Unknown";
}

static auto AttributeTypeFromName(const std::string &name) -> std::string {
  std::string lower = name;
  std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
  if (lower.find("pos") != std::string::npos)
    return "lumina::core::VertexAttributeType::Position";
  if (lower.find("norm") != std::string::npos ||
      lower.find("nrm") != std::string::npos)
    return "lumina::core::VertexAttributeType::Normal";
  if (lower.find("color") != std::string::npos ||
      lower.find("col") != std::string::npos)
    return "lumina::core::VertexAttributeType::Color";
  if (lower.find("tex") != std::string::npos ||
      lower.find("uv") != std::string::npos)
    return "lumina::core::VertexAttributeType::TexCoord";
  ASSERT(false, "Unrecognized vertex input variable name — add a naming "
                "convention match");
  return "lumina::core::VertexAttributeType::Position";
}

static auto ElementTypeFromFormat(SpvReflectFormat format) -> std::string {
  switch (format) {
    case SPV_REFLECT_FORMAT_R32_SFLOAT:
      return "lumina::core::ElementType::Float";
    case SPV_REFLECT_FORMAT_R32G32_SFLOAT:
      return "lumina::core::ElementType::Vec2";
    case SPV_REFLECT_FORMAT_R32G32B32_SFLOAT:
      return "lumina::core::ElementType::Vec3";
    case SPV_REFLECT_FORMAT_R32G32B32A32_SFLOAT:
      return "lumina::core::ElementType::Vec4";
    default:
      ASSERT(false, "Unsupported vertex input format");
      return "lumina::core::ElementType::Float";
  }
}

// --- Binding name / descriptor-write helpers ---

// Returns a usable name for a binding, falling back to the block type name
// when the GLSL variable name is empty (anonymous uniform blocks).
static auto GetBindingName(const SpvReflectDescriptorBinding *binding)
    -> std::string {
  if (binding->name && binding->name[0] != '\0') {
    return binding->name;
  }
  if (binding->type_description && binding->type_description->type_name &&
      binding->type_description->type_name[0] != '\0') {
    return binding->type_description->type_name;
  }
  return "binding" + std::to_string(binding->binding);
}

// Returns the field-name prefix for BindingData struct members (camelCase).
static auto GetBindingFieldPrefix(const SpvReflectDescriptorBinding *binding)
    -> std::string {
  std::string name = GetBindingName(binding);
  if (!name.empty()) {
    name[0] = static_cast<char>(::tolower(name[0]));
  }
  return name;
}

static auto VkDescriptorTypeString(SpvReflectDescriptorType type)
    -> std::string {
  switch (type) {
    case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:
      return "VK_DESCRIPTOR_TYPE_SAMPLER";
    case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
      return "VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER";
    case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
      return "VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE";
    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:
      return "VK_DESCRIPTOR_TYPE_STORAGE_IMAGE";
    case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
      return "VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER";
    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
      return "VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER";
    case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
      return "VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER";
    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
      return "VK_DESCRIPTOR_TYPE_STORAGE_BUFFER";
    default:
      ASSERT(false, "Unknown Vulkan descriptor type");
      return "VK_DESCRIPTOR_TYPE_MAX_ENUM";
  }
}

static auto IsBufferDescriptor(SpvReflectDescriptorType type) -> bool {
  return type == SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
         type == SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER;
}

static auto IsImageDescriptor(SpvReflectDescriptorType type) -> bool {
  return type == SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
         type == SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
         type == SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
         type == SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER;
}

static auto ReadSpvFile(const std::string &path) -> std::vector<uint32_t> {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file) {
    std::println(stderr, "reflect_tool: cannot open file: {}", path);
    std::exit(1);
  }
  const auto byte_size = static_cast<size_t>(file.tellg());
  file.seekg(0);
  std::vector<uint32_t> words(byte_size / sizeof(uint32_t));
  file.read(reinterpret_cast<char *>(words.data()),
            static_cast<std::streamsize>(byte_size));
  return words;
}

auto main(int argc, char **argv) -> int {
  if (argc < 3) {
    std::println(stderr, "Usage: reflect_tool <input.spv> <output.gen.hpp>");
    return 1;
  }

  const std::string input_path = argv[1];
  const std::string output_path = argv[2];

  // "shader.frag.spv" -> shader_name="shader", stage="frag"
  auto stem =
      std::filesystem::path(input_path).filename().stem(); // "shader.frag"
  std::string shader_name = stem.stem().string();          // "shader"
  std::string stage = stem.extension().string();           // ".frag"
  if (!stage.empty() && stage.front() == '.')
    stage = stage.substr(1); // "frag"

  const auto spv = ReadSpvFile(input_path);

  SpvReflectShaderModule module{};
  if (spvReflectCreateShaderModule(spv.size() * sizeof(uint32_t), spv.data(),
                                   &module) != SPV_REFLECT_RESULT_SUCCESS) {
    std::println(stderr, "reflect_tool: SPIRV-Reflect failed on: {}",
                 input_path);
    return 1;
  }

  // Count total bindings across all descriptor sets
  uint32_t total_binding_count = 0;
  for (uint32_t i = 0; i < module.descriptor_set_count; ++i) {
    total_binding_count += module.descriptor_sets[i].binding_count;
  }

  // Populated during push constant struct generation pass below
  std::string push_constant_struct_name;
  uint32_t push_constant_offset = 0;

  // Populated during vertex input variable pass below (vertex stage only)
  uint32_t vertex_input_count = 0;

  // --- Code generation ---

  cppgen::CodeUnit code;
  code.Add<cppgen::RawText>("#pragma once");
  code.Add<cppgen::RawText>(
      "// Auto-generated by spirv_reflect_tool — do not edit.");
  code.Add<cppgen::NewLine>();

  code.Add<cppgen::Include>("renderer/shaders/shader_layout.hpp", true);
  code.Add<cppgen::Include>("math/vector.hpp", true);
  code.Add<cppgen::Include>("math/matrix.hpp", true);
  if (total_binding_count > 0) {
    code.Add<cppgen::Include>("vulkan/vulkan.h", false);
  }
  code.Add<cppgen::NewLine>();

  // namespace syntax: "lumina::shaders::shader::frag"
  const std::string ns_name = "lumina::shaders::" + shader_name + "::" + stage;
  auto &ns = code.Add<cppgen::Namespace>(ns_name);

  // 1. UBO structs — must come before kBindings which references
  // sizeof(StructName)
  for (uint32_t i = 0; i < module.descriptor_set_count; ++i) {
    auto *set = &module.descriptor_sets[i];
    for (uint32_t j = 0; j < set->binding_count; ++j) {
      auto *binding = set->bindings[j];
      if (binding->descriptor_type !=
          SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
        continue;
      }

      const std::string struct_name = binding->type_description->type_name;
      auto &struct_ = ns.Add<cppgen::Struct>(struct_name);

      uint32_t current_offset = 0;
      int pad_index = 0;

      for (uint32_t k = 0; k < binding->block.member_count; ++k) {
        auto *member = &binding->block.members[k];

        // Gap before this member (e.g. explicit alignment padding)
        if (member->offset > current_offset) {
          struct_.Add<cppgen::ArrayVariable>(
              "uint8_t", "_pad" + std::to_string(pad_index++),
              std::to_string(member->offset - current_offset));
        }

        auto type = DecideType(member->type_description);
        struct_.Add<cppgen::Variable>(TypeName(type), member->name);
        current_offset = member->offset + member->size;

        // Trailing padding between this member and the next
        if (member->padded_size > member->size) {
          const uint32_t pad_bytes = member->padded_size - member->size;
          struct_.Add<cppgen::ArrayVariable>(
              "uint8_t", "_pad" + std::to_string(pad_index++),
              std::to_string(pad_bytes));
          current_offset += pad_bytes;
        }
      }

      // Tail padding to reach the declared block size
      if (current_offset < binding->block.size) {
        struct_.Add<cppgen::ArrayVariable>(
            "uint8_t", "_tail_pad",
            std::to_string(binding->block.size - current_offset));
      }

      ns.Add<cppgen::NewLine>();
    }
  }

  // 2. Push constant structs — same member+padding logic as UBO structs
  for (uint32_t i = 0; i < module.push_constant_block_count; ++i) {
    auto *block = &module.push_constant_blocks[i];
    push_constant_struct_name = block->type_description->type_name;
    push_constant_offset = block->offset;
    auto &struct_ = ns.Add<cppgen::Struct>(push_constant_struct_name);

    uint32_t current_offset = 0;
    int pad_index = 0;

    for (uint32_t k = 0; k < block->member_count; ++k) {
      auto *member = &block->members[k];

      if (member->offset > current_offset) {
        struct_.Add<cppgen::ArrayVariable>(
            "uint8_t", "_pad" + std::to_string(pad_index++),
            std::to_string(member->offset - current_offset));
      }

      auto type = DecideType(member->type_description);
      struct_.Add<cppgen::Variable>(TypeName(type), member->name);
      current_offset = member->offset + member->size;

      if (member->padded_size > member->size) {
        const uint32_t pad_bytes = member->padded_size - member->size;
        struct_.Add<cppgen::ArrayVariable>("uint8_t",
                                           "_pad" + std::to_string(pad_index++),
                                           std::to_string(pad_bytes));
        current_offset += pad_bytes;
      }
    }

    if (current_offset < block->padded_size) {
      struct_.Add<cppgen::ArrayVariable>(
          "uint8_t", "_tail_pad",
          std::to_string(block->padded_size - current_offset));
    }

    ns.Add<cppgen::NewLine>();
  }

  // 3. kBindings array (only if this stage has any descriptor bindings)
  if (total_binding_count > 0) {
    auto &bindings_array = ns.Add<cppgen::ArrayVariable>(
        "lumina::renderer::ShaderBindingInfo", "kBindings");
    bindings_array.AddSpecifiers("static", "constexpr");

    for (uint32_t i = 0; i < module.descriptor_set_count; ++i) {
      auto *set = &module.descriptor_sets[i];
      for (uint32_t j = 0; j < set->binding_count; ++j) {
        auto *binding = set->bindings[j];
        const bool is_ubo = binding->descriptor_type ==
                            SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        const std::string block_size =
            is_ubo ? ("sizeof(" +
                      std::string(binding->type_description->type_name) + ")")
                   : "0";

        auto &entry = bindings_array.AddEntry();
        entry.AddValue("set", std::to_string(binding->set));
        entry.AddValue("binding", std::to_string(binding->binding));
        entry.AddValue("type", "lumina::renderer::DescriptorBindingType::" +
                                   BindingTypeString(binding->descriptor_type));
        entry.AddValue("name", "\"" + GetBindingName(binding) + "\"");
        entry.AddValue("block_size", block_size);
        entry.AddValue("array_count", std::to_string(binding->count));
      }
    }
    ns.Add<cppgen::NewLine>();
  }

  // 4. kVertexInputs array (vertex stage only)
  if (stage == "vert") {
    uint32_t input_var_count = 0;
    spvReflectEnumerateInputVariables(&module, &input_var_count, nullptr);
    std::vector<SpvReflectInterfaceVariable *> input_vars(input_var_count);
    spvReflectEnumerateInputVariables(&module, &input_var_count,
                                      input_vars.data());

    // Filter out built-ins (gl_VertexIndex, gl_Position, etc.)
    std::vector<SpvReflectInterfaceVariable *> user_inputs;
    for (auto *var : input_vars) {
      if ((var->decoration_flags & SPV_REFLECT_DECORATION_BUILT_IN) != 0u)
        continue;
      user_inputs.push_back(var);
    }

    // Sort by location ascending
    std::sort(user_inputs.begin(), user_inputs.end(),
              [](auto *a, auto *b) { return a->location < b->location; });

    vertex_input_count = static_cast<uint32_t>(user_inputs.size());

    if (vertex_input_count > 0) {
      auto &inputs_array = ns.Add<cppgen::ArrayVariable>(
          "lumina::renderer::VertexInputInfo", "kVertexInputs");
      inputs_array.AddSpecifiers("static", "constexpr");

      for (auto *var : user_inputs) {
        auto &entry = inputs_array.AddEntry();
        entry.AddValue("location", std::to_string(var->location));
        entry.AddValue("attribute_type", AttributeTypeFromName(var->name));
        entry.AddValue("element_type", ElementTypeFromFormat(var->format));
      }
      ns.Add<cppgen::NewLine>();
    }
  }

  // 5. kLayout constexpr variable
  {
    const std::string bindings_ptr =
        (total_binding_count > 0) ? "kBindings" : "nullptr";
    const std::string push_constant_size_str =
        push_constant_struct_name.empty()
            ? "0"
            : ("sizeof(" + push_constant_struct_name + ")");
    const std::string vertex_input_layout_str =
        (vertex_input_count > 0)
            ? ("{ .input_count = " + std::to_string(vertex_input_count) +
               ", .inputs = kVertexInputs }")
            : "{}";
    const std::string layout_init =
        "{ .stage = " + StageEnumString(stage) +
        ", .binding_count = " + std::to_string(total_binding_count) +
        ", .bindings = " + bindings_ptr +
        ", .push_constant_size = " + push_constant_size_str +
        ", .push_constant_offset = " + std::to_string(push_constant_offset) +
        ", .vertex_input_layout = " + vertex_input_layout_str + " }";

    auto &layout_var =
        ns.Add<cppgen::Variable>("lumina::renderer::ShaderLayout", "kLayout");
    layout_var.AddSpecifiers("static", "constexpr");
    layout_var.SetInitializer(layout_init);
  }

  // 6. BindingData struct + WriteDescriptors function
  //    (only generated when the interface declares descriptor bindings)
  if (total_binding_count > 0) {
    // Collect all bindings with their field-name prefixes
    struct BindingGen {
      const SpvReflectDescriptorBinding *binding;
      std::string prefix;
    };
    std::vector<BindingGen> gen_bindings;
    for (uint32_t i = 0; i < module.descriptor_set_count; ++i) {
      auto *set = &module.descriptor_sets[i];
      for (uint32_t j = 0; j < set->binding_count; ++j) {
        gen_bindings.push_back(
            {set->bindings[j], GetBindingFieldPrefix(set->bindings[j])});
      }
    }

    ns.Add<cppgen::NewLine>();

    // 6a. BindingData struct — one field group per binding
    auto &bd = ns.Add<cppgen::Struct>("BindingData");
    for (const auto &[b, prefix] : gen_bindings) {
      switch (b->descriptor_type) {
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER: {
          bd.Add<cppgen::Variable>("VkBuffer", prefix + "_buffer");
          bd.Add<cppgen::Variable>("VkDeviceSize", prefix + "_offset")
              .SetInitializer("0");
          const auto range_default =
              b->descriptor_type ==
                      SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER
                  ? ("sizeof(" +
                     std::string(b->type_description->type_name) + ")")
                  : "VK_WHOLE_SIZE";
          bd.Add<cppgen::Variable>("VkDeviceSize", prefix + "_range")
              .SetInitializer(range_default);
          break;
        }
        case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: {
          bd.Add<cppgen::Variable>("VkSampler", prefix + "_sampler");
          bd.Add<cppgen::Variable>("VkImageView", prefix + "_imageView");
          bd.Add<cppgen::Variable>("VkImageLayout", prefix + "_imageLayout")
              .SetInitializer("VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL");
          break;
        }
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE: {
          bd.Add<cppgen::Variable>("VkImageView", prefix + "_imageView");
          bd.Add<cppgen::Variable>("VkImageLayout", prefix + "_imageLayout")
              .SetInitializer("VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL");
          break;
        }
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE: {
          bd.Add<cppgen::Variable>("VkImageView", prefix + "_imageView");
          bd.Add<cppgen::Variable>("VkImageLayout", prefix + "_imageLayout")
              .SetInitializer("VK_IMAGE_LAYOUT_GENERAL");
          break;
        }
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER: {
          bd.Add<cppgen::Variable>("VkSampler", prefix + "_sampler");
          break;
        }
        default:
          ASSERT(false,
                 "Unsupported descriptor type for BindingData generation");
          break;
      }
    }

    ns.Add<cppgen::NewLine>();

    // 6b. WriteDescriptors inline function
    auto &fn = ns.Add<cppgen::Function>("void", "WriteDescriptors");
    fn.AddSpecifiers("inline");
    fn.AddParameter("VkDevice", "device");
    fn.AddParameter("VkDescriptorSet", "set");
    fn.AddParameter("const BindingData&", "data");

    // Build function body as raw text (clang-format normalises indentation)
    std::string body;

    // Emit VkDescriptor*Info locals
    for (size_t idx = 0; idx < gen_bindings.size(); ++idx) {
      const auto &[b, prefix] = gen_bindings[idx];
      const auto s = std::to_string(idx);

      switch (b->descriptor_type) {
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
          body += "VkDescriptorBufferInfo buffer_info_" + s + "{\n";
          body += ".buffer = data." + prefix + "_buffer,\n";
          body += ".offset = data." + prefix + "_offset,\n";
          body += ".range = data." + prefix + "_range,\n";
          body += "};\n";
          break;
        case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
          body += "VkDescriptorImageInfo image_info_" + s + "{\n";
          body += ".sampler = data." + prefix + "_sampler,\n";
          body += ".imageView = data." + prefix + "_imageView,\n";
          body += ".imageLayout = data." + prefix + "_imageLayout,\n";
          body += "};\n";
          break;
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:
          body += "VkDescriptorImageInfo image_info_" + s + "{\n";
          body += ".sampler = VK_NULL_HANDLE,\n";
          body += ".imageView = data." + prefix + "_imageView,\n";
          body += ".imageLayout = data." + prefix + "_imageLayout,\n";
          body += "};\n";
          break;
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:
          body += "VkDescriptorImageInfo image_info_" + s + "{\n";
          body += ".sampler = data." + prefix + "_sampler,\n";
          body += ".imageView = VK_NULL_HANDLE,\n";
          body += ".imageLayout = VK_IMAGE_LAYOUT_UNDEFINED,\n";
          body += "};\n";
          break;
        default:
          break;
      }
    }

    // Emit VkWriteDescriptorSet array
    body += "VkWriteDescriptorSet writes[] = {\n";
    for (size_t idx = 0; idx < gen_bindings.size(); ++idx) {
      const auto &[b, prefix] = gen_bindings[idx];
      const auto s = std::to_string(idx);

      body += "{\n";
      body += ".sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,\n";
      body += ".dstSet = set,\n";
      body += ".dstBinding = " + std::to_string(b->binding) + ",\n";
      body += ".descriptorCount = 1,\n";
      body += ".descriptorType = " +
              VkDescriptorTypeString(b->descriptor_type) + ",\n";

      if (IsImageDescriptor(b->descriptor_type)) {
        body += ".pImageInfo = &image_info_" + s + ",\n";
      } else if (IsBufferDescriptor(b->descriptor_type)) {
        body += ".pBufferInfo = &buffer_info_" + s + ",\n";
      }

      body += "},\n";
    }
    body += "};\n";

    // Single batched call
    body += "vkUpdateDescriptorSets(device, " +
            std::to_string(gen_bindings.size()) + ", writes, 0, nullptr);";

    fn.Add<cppgen::RawText>(body);
  }

  const auto code_string = code.EmitCode();

  std::ofstream output_file(output_path);
  output_file << code_string;

  spvReflectDestroyShaderModule(&module);

  std::println(stdout, "reflect_tool: {} -> {}", input_path, output_path);
  return 0;
}
