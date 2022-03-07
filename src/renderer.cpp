#include "renderer.h"

#include "core.h"
#include "gfx/graphics_pass.h"

namespace rune {

Renderer::Renderer(Core& core) : core_(core), gfx_(core_.get_platform().get_graphics_backend()) {}

void Renderer::add_to_frame(const RenderObject& robj) {
    render_objects_by_mesh_[robj.mesh.get_id()].emplace_back(robj);
}

void Renderer::render() {
    // TODO: use shaderc to compile shader strings for fast iteration and so we're not committing spriv

    gfx::GraphicsPassDesc pass_desc = {};
    pass_desc.render_area      = {0, 0, core_.get_config().get_window_width(), core_.get_config().get_window_height()};
    pass_desc.vert_shader_path = "../data/shaders/triangle.vert.spv";
    pass_desc.frag_shader_path = "../data/shaders/triangle.frag.spv";

    static gfx::GraphicsPass pass(core_, gfx_, pass_desc);

    // TODO: materials
    // TODO: attachment description
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
    // create batches and object data
    std::vector<gfx::ObjectData> object_data;
    std::vector<gfx::MeshBatch>  batches;
    gfx::MeshBatch               prev_batch;
    for (auto [mesh_id, render_objects] : render_objects_by_mesh_) {
        gfx::MeshBatch batch;
        batch.mesh             = render_objects.front().mesh;
        batch.first_object_idx = prev_batch.first_object_idx + prev_batch.num_objects;
        batch.num_objects      = render_objects.size();
        batches.emplace_back(batch);

        prev_batch = batch;

        for (const RenderObject& robj : render_objects) {
            gfx::ObjectData odata = {};
            odata.model_matrix    = robj.model_matrix;
            object_data.emplace_back(odata);
        }
    }
    gfx_.update_object_data(object_data);
    geometry_batch_group_ = gfx_.add_batches(batches);
}

void Renderer::reset_frame() {
    render_objects_by_mesh_.clear();
}

} // namespace rune
