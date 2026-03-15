# Material & Shader Management System Design

## Overview

The material system sits between the renderer and the user-facing engine. It owns shader loading, SPIR-V reflection, descriptor set layout creation, pipeline creation, and per-instance GPU resource management. The design follows a **template + instance** pattern with **SPIR-V reflection** driving layout creation automatically.

---

## Layers

### Layer 1: `ShaderLayout` (reflection output)

Produced by reflecting a SPIR-V module via **spirv-cross**. Describes everything a pipeline needs to know about its inputs:

```cpp
enum class ShaderParamType { UniformBuffer, CombinedImageSampler, StorageBuffer };

struct DescriptorBindingInfo {
    u32 set;
    u32 binding;
    ShaderParamType type;
    VkShaderStageFlags stage_flags;
    std::string name;

    // For UBOs: member layout
    struct MemberInfo {
        std::string name;
        u32 offset;
        u32 size;
    };
    std::vector<MemberInfo> members; // populated for UniformBuffer
    u32 block_size = 0;              // total UBO size in bytes
};

struct ShaderLayout {
    std::vector<DescriptorBindingInfo> bindings; // all sets/bindings
    // Vertex input reflection (optional, may come from VertexBufferLayout instead)
};
```

Utility that runs spirv-cross and fills a `ShaderLayout`:
```cpp
auto ReflectShader(std::span<const u32> spirv, VkShaderStageFlagBits stage) -> ShaderLayout;
```

---

### Layer 2: `ShaderProgram`

Holds the raw SPIR-V for both stages and the **merged** layout (vert + frag bindings combined, stage flags OR'd for shared bindings):

```cpp
struct ShaderProgram {
    std::vector<u32> vert_spirv;
    std::vector<u32> frag_spirv;
    ShaderLayout merged_layout; // vert + frag merged
};

// Loaded from files, layout filled automatically:
auto LoadShaderProgram(std::string_view vert_path, std::string_view frag_path) -> ShaderProgram;
```

---

### Layer 3: `MaterialTemplate`

The heavy GPU object — created once per unique shader. Owns:
- `VkDescriptorSetLayout[]` — one per set index found in the merged layout
- `VkPipelineLayout` — from descriptor set layouts + push constant ranges
- `GraphicsPipelineHandle` — full Vulkan pipeline
- `VertexBufferLayout` — vertex attribute description (can come from reflection or be supplied explicitly)
- The `ShaderProgram` it was built from

**Convention**: Set 0 is **engine-owned** (camera UBO, etc.). Set 1+ are **material-owned** and managed by this template.

```cpp
struct MaterialTemplateDesc {
    ShaderProgram shader_program;
    VertexBufferLayout vertex_layout; // explicit or derived from reflection
};

struct MaterialTemplate {
    ShaderProgram shader_program;
    VertexBufferLayout vertex_layout;

    std::vector<VkDescriptorSetLayout> descriptor_set_layouts; // index = set number
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    GraphicsPipelineHandle pipeline_handle;

    auto CreateInstance() -> MaterialInstance;
};

using MaterialTemplateHandle = ResourceHandle;
using MaterialTemplateManager = ResourceManager<MaterialTemplate>;
```

---

### Layer 4: `MaterialInstance` (CPU-side)

Lightweight, user-facing object. Stores parameter values by name. Does **not** own GPU resources directly — those live in `RenderMaterial`.

```cpp
using MaterialParamValue = std::variant<
    f32, math::Vec2, math::Vec3, math::Vec4,
    math::Mat4, RenderTextureHandle
>;

struct MaterialInstance {
    MaterialTemplateHandle template_handle;
    std::unordered_map<std::string, MaterialParamValue> params;

    RenderMaterialHandle render_material_handle;
    bool render_active = false; // true once GPU upload has been submitted

    auto SetFloat(std::string_view name, f32 value) -> void;
    auto SetVec4(std::string_view name, math::Vec4 value) -> void;
    auto SetTexture(std::string_view name, RenderTextureHandle handle) -> void;
    // ... other setters
};

using MaterialInstanceHandle = ResourceHandle;
using MaterialInstanceManager = ResourceManager<MaterialInstance>;
```

---

### Layer 5: `RenderMaterial` (GPU-side)

Lives in the renderer. Owns the actual Vulkan resources for one material instance:
- `VkDescriptorSet` — allocated from a per-template pool, for set 1
- Material UBO `VkBuffer` — persistently mapped (one per frame-in-flight, or a single with explicit sync)
- `RenderTextureHandle[]` — references to uploaded textures

```cpp
struct RenderMaterial {
    VkDescriptorSet descriptor_set = VK_NULL_HANDLE; // set 1 (material)
    VkBuffer ubo_buffer = VK_NULL_HANDLE;
    VmaAllocation ubo_allocation = VK_NULL_HANDLE;
    void *ubo_mapped = nullptr;

    std::vector<RenderTextureHandle> textures;
    bool ready = false;
};

using RenderMaterialHandle = ResourceHandle;
using RenderMaterialManager = ResourceManager<RenderMaterial>;
```

---

## Texture System (prerequisite)

Textures follow the same async upload pattern as meshes:

```cpp
// CPU-side
struct Texture {
    u32 width, height;
    u32 channels;
    std::vector<u8> pixels;

    RenderTextureHandle render_texture_handle;
    bool render_active = false;
};

// GPU-side
struct RenderTexture {
    VkImage image = VK_NULL_HANDLE;
    VkImageView image_view = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    bool ready = false;
};
```

Upload flow: same job-dispatch pattern as `RenderMesh` — acquire `CommandContext`, record staging→device copy, submit, completion callback sets `ready = true`.

---

## ECS Integration

`StaticMeshComponent` gains a `MaterialInstanceHandle`:

```cpp
struct StaticMeshComponent {
    StaticMeshHandle static_mesh_handle;
    MaterialInstanceHandle material_instance_handle; // NEW
};
```

`LuminaEngine` gains:
```cpp
MaterialTemplateManager material_template_manager;
MaterialInstanceManager material_instance_manager;
MaterialTemplateHandle default_material_template_handle; // replaces default_pipeline_handle
```

---

## Upload / Activation Flow

Each frame in `ExecuteFrame`:

1. **Texture activation** — for any `Texture` with `render_active == false`, dispatch job to upload via `CommandContext`.
2. **Material activation** — for any `MaterialInstance` with `render_active == false` and all referenced textures ready:
   - Allocate `RenderMaterial` (descriptor set + UBO buffer).
   - Write UBO data from `params` (using `ShaderLayout::MemberInfo` offsets).
   - Update descriptor set (UBO + combined image samplers).
   - Set `render_active = true`.
3. **Mesh activation** — existing flow, unchanged.
4. **Draw list construction** — same as now but also include `render_material_handle`.

---

## Draw Call Flow

In `RecordCommandBuffer`, per draw:

```
bind pipeline (from MaterialTemplate via RenderMesh::pipeline_handle or material_template)
bind descriptor set 0 (engine: camera UBO)  ← once per frame
bind descriptor set 1 (RenderMaterial::descriptor_set)
push_constants model matrix
vkCmdDrawIndexed
```

---

## Descriptor Pool Strategy

Each `MaterialTemplate` owns its own `VkDescriptorPool` sized for an expected max instance count (e.g. 256 per template). Instances are allocated from their template's pool. This avoids a single global pool becoming a bottleneck and simplifies pool lifetime management.

---

## File Layout (proposed)

```
lumina/src/renderer/
    texture.hpp                  -- Texture (CPU), RenderTexture (GPU), managers
    material_template.hpp        -- MaterialTemplate, MaterialTemplateDesc, manager
    material_instance.hpp        -- MaterialInstance, RenderMaterial, managers
    shader_program.hpp           -- ShaderProgram, ShaderLayout, ReflectShader
    shader_program.cpp           -- spirv-cross reflection impl
```

---

## Implementation Order

1. `Texture` / `RenderTexture` + async upload (needed by materials)
2. `ShaderProgram` + `ReflectShader` (spirv-cross)
3. `MaterialTemplate` creation (descriptor set layouts, pipeline layout, pipeline)
4. `RenderMaterial` allocation + descriptor set writes
5. `MaterialInstance` CPU parameter API
6. Wire into `StaticMeshComponent` + draw call recording
