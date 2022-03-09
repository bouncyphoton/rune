#include "graphics_pass.h"

#include "core.h"

#include <map>
#include <unordered_set>

namespace rune::gfx {

GraphicsPass::GraphicsPass(Core& core, GraphicsBackend& gfx, const GraphicsPassDesc& desc)
    : RenderPass(core, gfx, desc.get_shaders()), desc_(desc) {
    const std::unordered_map<std::string, u32>& outputs = get_fragment_shader_outputs();

    std::unordered_set<std::string> unreferenced_outputs;
    for (const auto& [name, location] : outputs) {
        unreferenced_outputs.emplace(name);
    }

    bool success = true;

    // remap attachment names to locations
    std::map<u32, GraphicsPassDesc::ViewFormatPair> attachments;
    for (const auto& [name, data] : desc_.color_outputs_) {
        auto it = outputs.find(name);
        if (it == outputs.end()) {
            core_.get_logger().warn("could not find '%' in fragment shader outputs", name);
            success = false;
        } else {
            attachments[it->second] = data;
            unreferenced_outputs.erase(name);
        }
    }

    // warn about unreferenced outputs
    for (const auto& name : unreferenced_outputs) {
        core_.get_logger().warn("unreferenced output in graphics pass: '%'", name);
        success = false;
    }

    // todo: more validation

    if (!success) {
        core_.get_logger().fatal("failed to create graphics pass due to above warnings");
    }

    // turn into vectors
    std::vector<VkImageView> views;
    std::vector<VkFormat>    formats;
    for (const auto& [idx, data] : attachments) {
        if (idx >= views.size()) {
            // TODO: figure out what to do with unused attachments
            views.resize(idx + 1, VK_NULL_HANDLE);
            formats.resize(idx + 1, VK_FORMAT_UNDEFINED);
        }

        views[idx]   = data.view;
        formats[idx] = data.format;
    }

    if (desc_.depth_output_) {
        views.emplace_back(desc_.depth_output_->view);
        formats.emplace_back(desc_.depth_output_->format);
    }
    // create clear values
    for (VkFormat format : formats) {
        VkClearValue clear_value = {};
        if (GraphicsBackend::is_depth_format(format)) {
            clear_value.depthStencil.depth = 1;
        } else {
            clear_value.color = {0, 0, 0, 1};
        }
        clear_values_.emplace_back(clear_value);
    }

    // create render pass
    render_pass_ = gfx_.create_render_pass(formats);

    // create framebuffer for outputs
    framebuffer_ = gfx_.create_framebuffer(render_pass_, desc_.render_area, views);

    // create the pipeline for this graphics pass
    pipeline_ = gfx_.create_graphics_pipeline(desc_.get_shaders(), pipeline_layout_, render_pass_);
}

void GraphicsPass::run(VkCommandBuffer cmd, const std::function<void(VkCommandBuffer)>& func) {
    VkRenderPassBeginInfo begin_info = {};
    begin_info.sType                 = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin_info.renderPass            = render_pass_;
    begin_info.framebuffer           = framebuffer_;
    begin_info.renderArea            = desc_.render_area;
    begin_info.clearValueCount       = clear_values_.size();
    begin_info.pClearValues          = clear_values_.data();

    vkCmdBeginRenderPass(cmd, &begin_info, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport flipped_viewport = {};
    flipped_viewport.x          = (f32)desc_.render_area.offset.x;
    flipped_viewport.y          = (f32)desc_.render_area.offset.y + (f32)desc_.render_area.extent.height;
    flipped_viewport.width      = (f32)desc_.render_area.extent.width;
    flipped_viewport.height     = -1.0f * (f32)desc_.render_area.extent.height;
    flipped_viewport.minDepth   = 0.0f;
    flipped_viewport.maxDepth   = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &flipped_viewport);

    vkCmdSetScissor(cmd, 0, 1, &desc_.render_area);

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
