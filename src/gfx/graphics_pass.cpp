#include "graphics_pass.h"

#include "core.h"
#include "gfx/graphics_backend.h"
#include "utils.h"

#include <set>

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

void GraphicsPass::set_descriptors(VkCommandBuffer cmd, const DescriptorWrites& variable_writes) {
    struct SetWriteData {
        VkDescriptorSet                   descriptor_set;
        std::vector<VkWriteDescriptorSet> writes;
    };

    // name -> descriptor info
    const std::unordered_map<std::string, DescriptorInfo>& descriptors = get_descriptors();

    // set index -> descriptor writes
    std::unordered_map<u32, SetWriteData> set_writes;
    for (const auto& [variable_name, write_data] : variable_writes.get_write_data()) {
        auto it = descriptors.find(variable_name);
        if (it == descriptors.end()) {
            core_.get_logger().fatal("tried to set descriptor that doesn't exist: '%'", variable_name);
        }

        if (it->second.type != write_data.descriptor_type) {
            core_.get_logger().fatal("tried to write incorrect descriptor type: expected '%', got '%'",
                                     it->second.type,
                                     write_data.descriptor_type);
        }

        u32 set_idx = it->second.set;

        // see if we've written something to this set yet in this batch
        VkDescriptorSet set = set_writes[set_idx].descriptor_set;
        if (set == VK_NULL_HANDLE) {
            // if this is the first time we're seeing this set, get or allocate a new descriptor set
            set = gfx_.get_descriptor_set(get_descriptor_set_layout(set_idx));
        }

        VkWriteDescriptorSet write = {};
        write.sType                = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet               = set;
        write.dstBinding           = it->second.binding;
        write.descriptorType       = it->second.type;
        write.descriptorCount      = 1;

        switch (write_data.write_type) {
        case DescriptorWrites::Write::WriteDataType::BUFFER:
            write.pBufferInfo = &write_data.data.buffer_info;
            break;
        case DescriptorWrites::Write::WriteDataType::IMAGE:
            write.pImageInfo = &write_data.data.image_info;
            break;
        case DescriptorWrites::Write::WriteDataType::INVALID:
            core_.get_logger().fatal("invalid write type");
            break;
        }

        // TODO: ...

        set_writes[set_idx].descriptor_set = set;
        set_writes[set_idx].writes.emplace_back(write);
    }

    for (auto [set_idx, write_data] : set_writes) {
        gfx_.update_descriptor_sets(write_data.writes);
        vkCmdBindDescriptorSets(cmd,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipeline_layout_,
                                set_idx,
                                1,
                                &write_data.descriptor_set,
                                0,
                                nullptr);
    }
}

} // namespace rune::gfx
