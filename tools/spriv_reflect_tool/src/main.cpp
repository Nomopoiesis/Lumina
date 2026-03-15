#define CPPGEN_IMPLEMENTATION
#include <cppgen/cppgen.hpp>
#include <spirv_reflect.h>

#include "lumina_assert.hpp"
#include "lumina_types.hpp"

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
  ASSERT(false, "Unknown shader stage");
  return "Unknown";
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

  // --- Code generation ---

  cppgen::CodeUnit code;
  code.Add<cppgen::RawText>("#pragma once");
  code.Add<cppgen::RawText>(
      "// Auto-generated by spirv_reflect_tool — do not edit.");
  code.Add<cppgen::NewLine>();

  code.Add<cppgen::Include>("renderer/shaders/shader_layout.hpp", true);
  code.Add<cppgen::Include>("math/vector.hpp", true);
  code.Add<cppgen::Include>("math/matrix.hpp", true);
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
        struct_.Add<cppgen::ArrayVariable>(
            "uint8_t", "_pad" + std::to_string(pad_index++),
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
        entry.AddValue("type", "lumina::renderer::BindingType::" +
                                   BindingTypeString(binding->descriptor_type));
        entry.AddValue("name", "\"" + std::string(binding->name) + "\"");
        entry.AddValue("block_size", block_size);
        entry.AddValue("array_count", std::to_string(binding->count));
      }
    }
    ns.Add<cppgen::NewLine>();
  }

  // 4. kLayout constexpr variable
  {
    const std::string bindings_ptr =
        (total_binding_count > 0) ? "kBindings" : "nullptr";
    const std::string push_constant_size_str =
        push_constant_struct_name.empty()
            ? "0"
            : ("sizeof(" + push_constant_struct_name + ")");
    const std::string layout_init =
        "{ .stage = " + StageEnumString(stage) +
        ", .bindings = " + bindings_ptr +
        ", .binding_count = " + std::to_string(total_binding_count) +
        ", .push_constant_size = " + push_constant_size_str +
        ", .push_constant_offset = " + std::to_string(push_constant_offset) +
        " }";

    auto &layout_var =
        ns.Add<cppgen::Variable>("lumina::renderer::ShaderLayout", "kLayout");
    layout_var.AddSpecifiers("static", "constexpr");
    layout_var.SetInitializer(layout_init);
  }

  const auto code_string = code.EmitCode();

  std::ofstream output_file(output_path);
  output_file << code_string;

  spvReflectDestroyShaderModule(&module);

  std::println(stdout, "reflect_tool: {} -> {}", input_path, output_path);
  return 0;
}
