#include "core.h"

#include <GLFW/glfw3.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <filesystem>
#include <tiny_obj_loader.h>

namespace rune {

struct Model {
    explicit Model(const std::vector<gfx::Mesh>& meshes = {}, const std::vector<u32>& material_ids = {})
        : meshes_(meshes), material_ids_(material_ids) {
        if (!material_ids.empty()) {
            rune_assert_eq(material_ids.size(), meshes.size());
        }
    }

    void add_to_scene(Renderer& renderer, glm::mat4 transformation) const {
        if (meshes_.empty()) {
            return;
        }

        RenderObject robj = {};
        robj.model_matrix = transformation;

        for (u32 i = 0; i < meshes_.size(); ++i) {
            robj.mesh = meshes_[i];
            if (!material_ids_.empty()) {
                robj.material_id = material_ids_[i];
            } else {
                robj.material_id = 0;
            }
            renderer.add_to_frame(robj);
        }
    }

  private:
    std::vector<gfx::Mesh> meshes_;
    std::vector<u32>       material_ids_;
};

static Model load_model(Core& core, const std::string& path) {
    // core.get_logger().info("loading model: '%'", path);

    tinyobj::attrib_t                attrib;
    std::vector<tinyobj::shape_t>    shapes;
    std::vector<tinyobj::material_t> materials;
    std::string                      warn;
    std::string                      err;

    std::string mtl_basedir = std::filesystem::path(path).parent_path().string();

    bool success = tinyobj::LoadObj(&attrib,
                                    &shapes,
                                    &materials,
                                    &warn,
                                    &err,
                                    path.c_str(),
                                    mtl_basedir.c_str());
    if (!warn.empty()) {
        core.get_logger().warn("tinyobj warning: %", warn);
    }
    if (!err.empty()) {
        core.get_logger().warn("tinyobj error: %", err);
    }
    if (!success) {
        core.get_logger().fatal("failed to load model: '%'", path);
    }

    std::vector<std::vector<Vertex>> mesh_data;

    // go over shapes
    for (auto& shape : shapes) {
        // go over faces
        u32 index_offset = 0;
        for (u32 f = 0; f < shape.mesh.num_face_vertices.size(); ++f) {
            u32 fv = shape.mesh.num_face_vertices[f];
            rune_assert(fv == 3); // must be triangle

            i32 mat_id = shape.mesh.material_ids[f];
            if (mat_id >= mesh_data.size()) {
                mesh_data.resize(mat_id + 1);
            }
            auto& vertices = mesh_data[mat_id];

            // loop over vertices
            for (u32 v = 0; v < fv; ++v) {
                tinyobj::index_t idx = shape.mesh.indices[index_offset + v];

                tinyobj::real_t x = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
                tinyobj::real_t y = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
                tinyobj::real_t z = attrib.vertices[3 * size_t(idx.vertex_index) + 2];

                f32 tx = 0, ty = 0;
                if (idx.texcoord_index >= 0) {
                    tx = attrib.texcoords[2 * size_t(idx.texcoord_index) + 0];
                    ty = 1.0 - attrib.texcoords[2 * size_t(idx.texcoord_index) + 1];
                }

                tinyobj::real_t nx = attrib.normals[3 * size_t(idx.normal_index) + 0];
                tinyobj::real_t ny = attrib.normals[3 * size_t(idx.normal_index) + 1];
                tinyobj::real_t nz = attrib.normals[3 * size_t(idx.normal_index) + 2];

                vertices.emplace_back(Vertex{{x, y, z}, {nx, ny, nz}, {tx, ty}});
            }
            index_offset += fv;
        }
    }

    gfx::GraphicsBackend& gfx_backend = core.get_platform().get_graphics_backend();

    // convert meshes and materials
    std::vector<gfx::Mesh> meshes;
    meshes.reserve(mesh_data.size());

    std::vector<u32> material_ids;
    material_ids.reserve(mesh_data.size());

    for (u32 mat_id = 0; mat_id < mesh_data.size(); ++mat_id) {
        std::vector<Vertex> const& vertices = mesh_data[mat_id];
        if (vertices.empty()) {
            continue;
        }
        meshes.emplace_back(gfx_backend.load_mesh(vertices));
        material_ids.emplace_back(
            gfx_backend.load_texture(mtl_basedir + "/" + materials[mat_id].diffuse_texname).id);
    }

    //core.get_logger().info("loaded % mesh% for model '%'", meshes.size(), (meshes.size() == 1 ? "" : "es"), path);

    return Model(meshes, material_ids);
}

struct Transformation {
    Transformation() : position(glm::vec3(0)), rotation(glm::vec3(0)), scale(glm::vec3(1)) {}

    glm::vec3 position;
    glm::vec3 rotation;
    glm::vec3 scale;

    [[nodiscard]] glm::mat4 get_matrix() const {
        return glm::translate(glm::mat4(1), position) * glm::rotate(glm::mat4(1), rotation.x, glm::vec3(1, 0, 0)) *
               glm::rotate(glm::mat4(1), rotation.y, glm::vec3(0, 1, 0)) *
               glm::rotate(glm::mat4(1), rotation.z, glm::vec3(0, 0, 1)) * glm::scale(glm::mat4(1), scale);
    }
};

Core::Core() : config_(*this), platform_(*this), renderer_(*this) {
    logger_.info("operating system: %", consts::os_name);
    logger_.info("is release build: %", consts::is_release);
}

void Core::run() {
    f32    aspect_ratio = (f32)config_.get_window_width() / (f32)config_.get_window_height();
    Camera camera(glm::half_pi<f32>(), aspect_ratio, 0.01f, 100.0f, glm::vec3(0, 1, 0));

    std::vector<Model> models;
    for (const auto& entry : std::filesystem::directory_iterator("../data/models/retro_urban_kit/obj")) {
        if (entry.path().extension() != ".obj") {
            continue;
        }
        models.emplace_back(load_model(*this, entry.path().string()));
    }

    // Add models
    const i32 num_models = (i32)models.size();
    const i32 side_length = std::ceil(std::sqrt(num_models));
    const i32 half_extent = std::ceil((f32)(side_length - 1) / 2.0f);
    constexpr float distance = 2.0f;
    for (i32 x = -half_extent; x <= half_extent; ++x) {
        for (i32 z = -half_extent; z <= half_extent; ++z) {
            Transformation transformation;
            transformation.position = glm::vec3(x, 0, z) * distance;

            const auto entity = registry_.create();
            registry_.emplace<Transformation>(entity, transformation);
            registry_.emplace<Model>(entity, models[(x * side_length + z) % models.size()]);
        }
    }

    while (running_) {
        platform_.update();
        if (platform_.is_key_down(GLFW_KEY_ESCAPE)) {
            stop();
        }

        f32 dt = platform_.get_delta_time();

        // player camera

        constexpr f32 slow_speed = 1.0f;
        constexpr f32 fast_speed = 10.0f;
        f32           move_speed = dt * (platform_.is_key_down(GLFW_KEY_LEFT_SHIFT) ? fast_speed : slow_speed);
        if (platform_.is_key_down(GLFW_KEY_W)) {
            camera.add_position(camera.get_forward() * move_speed);
        }
        if (platform_.is_key_down(GLFW_KEY_S)) {
            camera.add_position(camera.get_forward() * -move_speed);
        }
        if (platform_.is_key_down(GLFW_KEY_D)) {
            camera.add_position(camera.get_right() * move_speed);
        }
        if (platform_.is_key_down(GLFW_KEY_A)) {
            camera.add_position(camera.get_right() * -move_speed);
        }
        if (platform_.is_key_down(GLFW_KEY_SPACE)) {
            camera.add_position(consts::up * move_speed);
        }
        if (platform_.is_key_down(GLFW_KEY_LEFT_CONTROL)) {
            camera.add_position(consts::up * -move_speed);
        }
        if (platform_.is_key_pressed(GLFW_MOUSE_BUTTON_RIGHT)) {
            platform_.set_mouse_grabbed(true);
        }
        if (platform_.is_key_released(GLFW_MOUSE_BUTTON_RIGHT)) {
            platform_.set_mouse_grabbed(false);
        }
        if (platform_.is_mouse_grabbed()) {
            camera.add_pitch(platform_.get_mouse_delta().y / (f32)get_config().get_window_height());
            camera.add_yaw(platform_.get_mouse_delta().x / (f32)get_config().get_window_width());
        }
        renderer_.set_camera(camera);

        // render

        for (auto [entity, transformation, model] : registry_.view<Transformation, const Model>().each()) {
            model.add_to_scene(renderer_, transformation.get_matrix());
        }
        renderer_.render();
    }
}

} // namespace rune
