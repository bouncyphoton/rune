#include "renderer.h"

#include "core.h"
#include "gfx/graphics_pass.h"

#include <GLFW/glfw3.h>

namespace rune {

Renderer::Renderer(Core& core) : core_(core), gfx_(core_.get_platform().get_graphics_backend()) {}

void Renderer::render() {
    // TODO: use shaderc to compile shader strings for fast iteration and so we're not committing spriv

    gfx::GraphicsPassDesc pass_desc = {};
    pass_desc.render_area      = {0, 0, core_.get_config().get_window_width(), core_.get_config().get_window_height()};
    pass_desc.vert_shader_path = "../data/shaders/triangle.vert.spv";
    pass_desc.frag_shader_path = "../data/shaders/triangle.frag.spv";

    static gfx::GraphicsPass pass(core_, gfx_, pass_desc);

    gfx_.begin_frame();
    {
        // todo: unified buffers

        pass.run(gfx_.get_command_buffer(), [&](VkCommandBuffer cmd) {
            // update unified buffer descriptors
            // pass.update_descriptors();

            // pass.set_descriptors();

            // loop over renderables

            f32 time = (f32)glfwGetTime();
            pass.set_push_constants(cmd, VK_SHADER_STAGE_VERTEX_BIT, time);
            vkCmdDraw(cmd, 3, 1, 0, 0);
        });
    }
    gfx_.end_frame();
}

} // namespace rune