#include "renderer.h"

#include "core.h"

namespace rune {

Renderer::Renderer(Core& core) : core_(core), gfx_(core_.get_platform().get_graphics_backend()) {}

void Renderer::render() {
    // TODO: use shaderc to compile shader strings for fast iteration and so we're not committing spriv

    GraphicsPassDesc pass_desc = {};
    pass_desc.vert_shader_path = "../data/shaders/test.vert.spv";
    pass_desc.frag_shader_path = "../data/shaders/test.frag.spv";
    static GraphicsPass pass   = gfx_.create_pass(pass_desc);

    // maybe lazy allocate descriptor sets?

    // add to buffers but dont modify/delete anything in-flight
    // wait for fence

    // unified vertex buffer
    // unified index buffer
    // unified constant buffer

    gfx_.begin_frame();
    {
        gfx_.start_pass(pass);

#if 0
        VkCommandBuffer cmd = gfx_.get_command_buffer();

        VkDescriptorSet bindless_set;
        VkDescriptorSetAllocateInfo set_alloc_info = {};
        set_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        set_alloc_info.descriptorPool = ;
        set_alloc_info.descriptorSetCount = 1;
        set_alloc_info.pSetLayouts = &set_layout;
        vkAllocateDescriptorSets(device_, &set_alloc_info, &bindless_set);


        VkDescriptorBufferInfo buffer_info = {};
        buffer_info.buffer = vertices_buffer;
        buffer_info.offset = 0;
        buffer_info.range = ;

        VkWriteDescriptorSet write = {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write.dstSet = bindless_set;
        write.dstBinding = 0;
        write.pBufferInfo = &buffer_info;
        vkUpdateDescriptorSets(device_, 1, &writes, 0, nullptr);


        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &bindless_set, 0, nullptr);
#endif

        gfx_.end_pass();

#if 0
        // possible:
        Pipeline mesh_pipeline = gfx_.create_pipeline(mesh_pipeline_desc); //! vkCreateGraphicsPipeline



        gfx_.set_targets(wip_color, depth); //! creates render pass, subpass, pipeline layout
        gfx_.set_descriptor_set(/* set_idx */ 0, vertex_array, mesh_array, texture_array); //! write and vkCmdBindDescriptorSets

        gfx_.set_pipeline(mesh_pipeline); // vkCmdBindPipeline
        for (auto renderable : renderables) {
            gfx_.set_push_constants(renderable.mesh_idx, renderable.tranform); //! vkCmdPushConstants
            gfx_.draw(renderable.num_mesh_indices); //! vkCmdDraw
        }

        // transition image, undecided to go with 1 or 2 or something else
        // look at bloom example
        /* 1 */ gfx_.transition_image(wip_color, from COLOR_ATTACHMENT to SHADER_READ_ONLY_OPTIMAL, COLOR_ATTACHMENT_WRITE to SHADER_READ); //! vkCmdPipelineBarrier with VkImageMemoryBarrier
        /* 2 */ gfx_.transition_image(wip_color, COLOR_OUTPUT_TO_COLOR_ATTACHMENT);

        gfx_.set_targets(swapchain);
        gfx_.set_descriptor_set(1, wip_color); // set 1, where input data lives. set 0 is carried over

        gfx_.set_pipeline(post_process_pipeline);
        gfx_.set_push_constants(camera.aperture, camera.shutter_speed);
        gfx_.draw(3);
#endif
    }
    gfx_.end_frame();
}

} // namespace rune
