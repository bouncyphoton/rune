#include "renderer.h"

#include "core.h"
#include "gfx/graphics_pass.h"

namespace rune {

Renderer::Renderer(Core& core) : core_(core), gfx_(core_.get_platform().get_graphics_backend()) {}

void Renderer::add_to_frame(const RenderObject& robj) {
    render_objects_.emplace_back(robj);
}

void Renderer::render() {
    // TODO: use shaderc to compile shader strings for fast iteration and so we're not committing spriv

    gfx::GraphicsPassDesc pass_desc = {};
    pass_desc.render_area      = {0, 0, core_.get_config().get_window_width(), core_.get_config().get_window_height()};
    pass_desc.vert_shader_path = "../data/shaders/triangle.vert.spv";
    pass_desc.frag_shader_path = "../data/shaders/triangle.frag.spv";

    static gfx::GraphicsPass pass(core_, gfx_, pass_desc);

    // TODO: index buffer support

    process_object_data();

    gfx_.begin_frame();
    {
        pass.run(gfx_.get_command_buffer(), [&](VkCommandBuffer cmd) {
            // update unified buffer descriptors
            gfx::DescriptorWrites writes;
            writes.set_buffer("u_vertices", gfx_.get_unified_vertex_buffer());
            writes.set_buffer("u_object_data", gfx_.get_object_data_buffer());
            pass.set_descriptors(cmd, writes);

            struct DrawData {
                glm::mat4 vp;
            } draw_data  = {};
            draw_data.vp = camera_.get_view_projection_matrix();
            pass.set_push_constants(cmd, VK_SHADER_STAGE_VERTEX_BIT, draw_data);

            gfx_.draw_batch_group(cmd, geometry_batch_group_);
        });
    }
    gfx_.end_frame();

    reset_frame();
}

void Renderer::process_object_data() {
    // TODO: sort objects to be optimal for batching

    // update object data
    std::vector<gfx::ObjectData> object_data(render_objects_.size());
    for (u32 i = 0; i < render_objects_.size(); ++i) {
        object_data[i].model_matrix = render_objects_[i].model_matrix;
    }
    gfx_.update_object_data(object_data);

    // create batches
    std::vector<gfx::MeshBatch> batches;
    gfx::MeshBatch              current_batch;
    for (const auto& render_object : render_objects_) {
        if (render_object.mesh != current_batch.mesh) {
            if (current_batch.num_objects > 0) {
                batches.emplace_back(current_batch);
            }

            gfx::MeshBatch prev_batch      = current_batch;
            current_batch                  = gfx::MeshBatch();
            current_batch.mesh             = render_object.mesh;
            current_batch.first_object_idx = prev_batch.first_object_idx + prev_batch.num_objects;
        }

        ++current_batch.num_objects;
    }
    // get the left over batch
    if (current_batch.num_objects > 0) {
        batches.emplace_back(current_batch);
    }

    geometry_batch_group_ = gfx_.add_batches(batches);
}

void Renderer::reset_frame() {
    render_objects_.clear();
}

} // namespace rune
