#include "renderer.h"

#include "core.h"
#include "gfx/graphics_pass.h"

namespace rune {

Renderer::Renderer(Core& core) : core_(core), gfx_(core_.get_platform().get_graphics_backend()) {
    core_.get_logger().info("initializing renderer");
    u32 width  = core_.get_config().get_window_width();
    u32 height = core.get_config().get_window_height();
    color_tex_ = gfx_.create_output_texture(VK_FORMAT_R8G8B8A8_UNORM, width, height);
    depth_tex_ = gfx_.create_output_texture(VK_FORMAT_D32_SFLOAT, width, height);

    gfx::GraphicsPassDesc pass_desc = {};
    pass_desc.render_area      = {0, 0, width, height};
    pass_desc.vert_shader_path = "../data/shaders/triangle.vert.spv";
    pass_desc.frag_shader_path = "../data/shaders/triangle.frag.spv";
    pass_desc.add_color_output("o_img", *color_tex_);
    pass_desc.set_depth_output(*depth_tex_);
    main_pass_.emplace(core_, gfx_, pass_desc);

    u8 default_tex_data[] = {
        255, 0, 255, 255,
        0, 0, 0, 255,
        0, 0, 0, 255,
        255, 0, 255, 255,
    };
    default_tex_ = gfx_.create_sampled_texture(VK_FORMAT_R8G8B8A8_UNORM, 2, 2, default_tex_data, sizeof(default_tex_data)); // TODO: get missing texture from gfx
    core_.get_logger().info("initialized renderer");
}

void Renderer::add_to_frame(const RenderObject& robj) {
    render_objects_by_mesh_[robj.mesh.get_id()].emplace_back(robj);
}

void Renderer::render() {
    // TODO: index buffer support

    process_object_data();

    gfx_.begin_frame();
    draw_with_pass(*main_pass_);
    gfx_.end_frame(*color_tex_);
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
            odata.material_id     = robj.material_id;
            object_data.emplace_back(odata);
        }
    }
    gfx_.update_object_data(object_data);
    geometry_batch_group_ = gfx_.add_batches(batches);

    // reset queued up meshes
    render_objects_by_mesh_.clear();
}

void Renderer::draw_with_pass(gfx::GraphicsPass& pass) {
    pass.run(gfx_.get_command_buffer(), [&](VkCommandBuffer cmd) {
      // update unified buffer descriptors
      gfx::DescriptorWrites writes;
      writes.set_buffer("u_vertices", gfx_.get_unified_vertex_buffer());
      writes.set_buffer("u_object_data", gfx_.get_object_data_buffer());

      std::vector<gfx::Texture> const& textures = gfx_.get_loaded_textures();
      for (u32 i = 0; i < 128; ++i) {
          // TODO: don't set these all every frame and do real bindless textures
          VkImageView image_view = default_tex_->get_image_view();
          if (i < textures.size()) {
              image_view = textures[i].get_image_view();
          }
          writes.set_image_sampler("u_textures", gfx_.get_nearest_sampler(), image_view, i);
      }
      pass.set_descriptors(cmd, writes);

      struct DrawData {
          glm::mat4 vp;
      } draw_data  = {};
      draw_data.vp = camera_.get_view_projection_matrix();
      pass.set_push_constants(cmd, VK_SHADER_STAGE_VERTEX_BIT, draw_data);

      gfx_.draw_batch_group(cmd, geometry_batch_group_);
    });
}

} // namespace rune
