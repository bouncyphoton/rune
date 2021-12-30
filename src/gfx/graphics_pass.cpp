#include "graphics_pass.h"

#include "gfx/graphics_backend.h"
#include "utils.h"

namespace rune::gfx {

GraphicsPass::GraphicsPass(Core& core, GraphicsBackend& gfx, const GraphicsPassDesc& desc)
    : RenderPass(core, gfx, desc.get_shaders()), desc_(desc) {
    // TODO: allow creating a renderpass that doesn't present, like gbuffer

    render_pass_ = gfx_.create_render_pass();
    pipeline_    = gfx_.create_graphics_pipeline(desc_.get_shaders(), pipeline_layout_, render_pass_);
    gfx_.create_framebuffers(render_pass_, desc_.render_area);
}

void GraphicsPass::run(VkCommandBuffer cmd, const std::function<void(VkCommandBuffer)>& func) {
    VkClearValue clear_value = {};
    clear_value.color        = {0, 0, 0, 1};

    VkRenderPassBeginInfo begin_info = {};
    begin_info.sType                 = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin_info.renderPass            = render_pass_;
    begin_info.framebuffer           = gfx_.get_framebuffer(render_pass_);
    begin_info.renderArea            = desc_.render_area;
    begin_info.clearValueCount       = 1;
    begin_info.pClearValues          = &clear_value;

    vkCmdBeginRenderPass(cmd, &begin_info, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

    func(cmd);

    vkCmdEndRenderPass(cmd);
}

} // namespace rune::gfx
