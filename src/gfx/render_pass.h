#ifndef RUNE_RENDER_PASS_H
#define RUNE_RENDER_PASS_H

#include "consts.h"
#include "types.h"

#include <functional>
#include <string>
#include <vulkan/vulkan.h>

namespace rune {
class Core;
}

namespace rune::gfx {

class GraphicsBackend;

struct ShaderInfo {
    VkShaderStageFlagBits stage;
    const char*           path;
};

struct DescriptorWrites {
    // TODO
};

class RenderPass {
  public:
    explicit RenderPass(Core& core, GraphicsBackend& gfx, const std::vector<ShaderInfo>& shaders);
    virtual ~RenderPass();

    RenderPass(const RenderPass&) = delete;
    RenderPass& operator=(const RenderPass&) = delete;

    virtual void run(VkCommandBuffer cmd, const std::function<void(VkCommandBuffer)>& func) = 0;

    template <typename T>
    void set_push_constants(VkCommandBuffer cmd, VkShaderStageFlagBits shader_stage, const T& data, u32 offset = 0) {
        set_push_constants(cmd, shader_stage, &data, sizeof(data), offset);
    }

    void
    set_push_constants(VkCommandBuffer cmd, VkShaderStageFlagBits shader_stage, const void* data, u32 size, u32 offset);

  protected:
    Core&            core_;
    GraphicsBackend& gfx_;
    VkPipelineLayout pipeline_layout_;

  private:
    void process_shaders(const std::vector<ShaderInfo>& shaders);

    struct DescriptorInfo {
        u32              set;
        u32              binding;
        VkDescriptorType type;
    };

    struct PushConstantsInfo {
        u32                   offset;
        u32                   size;
        VkShaderStageFlagBits stage;

        bool operator==(const PushConstantsInfo& rhs) const {
            return std::tie(offset, size, stage) == std::tie(rhs.offset, rhs.size, rhs.stage);
        }
        bool operator!=(const PushConstantsInfo& rhs) const {
            return !(rhs == *this);
        }
    };

    std::unordered_map<std::string, DescriptorInfo> descriptors_;
    std::vector<PushConstantsInfo>                  push_constants_;
};

} // namespace rune::gfx

#endif // RUNE_RENDER_PASS_H
