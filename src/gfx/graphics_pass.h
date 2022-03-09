#ifndef RUNE_GRAPHICS_PASS_H
#define RUNE_GRAPHICS_PASS_H

#include "gfx/render_pass.h"
#include "texture.h"

namespace rune {
class Core;
}

namespace rune::gfx {

class GraphicsBackend;

struct GraphicsPassDesc {
    VkRect2D render_area = {0, 0};

    // temp shader paths

    const char* vert_shader_path = nullptr;
    const char* frag_shader_path = nullptr;

    [[nodiscard]] std::vector<ShaderInfo> get_shaders() const {
        return {{VK_SHADER_STAGE_VERTEX_BIT, vert_shader_path}, {VK_SHADER_STAGE_FRAGMENT_BIT, frag_shader_path}};
    }

    void add_color_output(const std::string& name, const rune::gfx::Texture& texture) {
        color_outputs_[name] = std::make_pair(texture.get_image_view(), texture.get_format());
    }

  private:
    friend class GraphicsPass;
    std::unordered_map<std::string, std::pair<VkImageView, VkFormat>> color_outputs_;
};

class GraphicsPass : public RenderPass {
  public:
    explicit GraphicsPass(Core& core, GraphicsBackend& gfx, const GraphicsPassDesc& desc);

    void run(VkCommandBuffer cmd, const std::function<void(VkCommandBuffer)>& func) override;

    void set_descriptors(VkCommandBuffer cmd, const DescriptorWrites& writes) override;

  private:
    GraphicsPassDesc          desc_;
    VkRenderPass              render_pass_;
    VkFramebuffer             framebuffer_;
    VkPipeline                pipeline_;
    std::vector<VkClearValue> clear_values_;
};

} // namespace rune::gfx

#endif // RUNE_GRAPHICS_PASS_H
