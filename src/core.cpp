#include "core.h"

#include <GLFW/glfw3.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

namespace rune {

struct Model {
    explicit Model(const std::vector<gfx::Mesh>& meshes = {}) : meshes_(meshes) {}

    void add_to_scene(Renderer& renderer, glm::mat4 transformation) const {
        if (meshes_.empty()) {
            return;
        }

        RenderObject robj = {};
        robj.model_matrix = transformation;

        for (const auto& mesh : meshes_) {
            robj.mesh = mesh;
            renderer.add_to_frame(robj);
        }
    }

  private:
    std::vector<gfx::Mesh> meshes_;
};

static Model load_model(Core& core, const std::string& path) {
    core.get_logger().info("loading model: '%'", path);

    tinyobj::attrib_t                attrib;
    std::vector<tinyobj::shape_t>    shapes;
    std::vector<tinyobj::material_t> materials;
    std::string                      warn;
    std::string                      err;

    bool success = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str());
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
            rune_assert(core, fv == 3); // must be triangle

            i32 mat_id = shape.mesh.material_ids[f] + 1;
            if (mat_id >= mesh_data.size()) {
                mesh_data.resize(mat_id + 1);
            }
            auto& mesh = mesh_data[mat_id];

            // loop over vertices
            for (u32 v = 0; v < fv; ++v) {
                tinyobj::index_t idx = shape.mesh.indices[index_offset + v];

                tinyobj::real_t x = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
                tinyobj::real_t y = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
                tinyobj::real_t z = attrib.vertices[3 * size_t(idx.vertex_index) + 2];

                f32 tx = 0, ty = 0;
                if (idx.texcoord_index >= 0) {
                    tx = attrib.texcoords[2 * size_t(idx.texcoord_index) + 0];
                    ty = attrib.texcoords[2 * size_t(idx.texcoord_index) + 1];
                }

                mesh.emplace_back(Vertex{x, y, z, tx, ty});
            }
            index_offset += fv;

            // also, material here
            // shapes[s].mesh.material_ids[f];
        }
    }

    core.get_logger().info("loaded % meshes for model '%'", mesh_data.size(), path);

    std::vector<gfx::Mesh> meshes;
    meshes.reserve(mesh_data.size());
    for (const std::vector<Vertex>& vertices : mesh_data) {
        meshes.emplace_back(core.get_platform().get_graphics_backend().load_mesh(vertices));
    }

    return Model(meshes);
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
    Camera camera(glm::half_pi<f32>(), aspect_ratio, 0.01f, 100.0f);

    // counterclockwise winding order

    Vertex triangle_vertices[] = {
        Vertex{-0.5f, -0.5f, 0, 0, 1},
        Vertex{0.5f, -0.5f, 0, 1, 1},
        Vertex{0, 0.5f, 0, 0.5f, 0},
    };
    gfx::Mesh triangle = platform_.get_graphics_backend().load_mesh(triangle_vertices, std::size(triangle_vertices));

    Vertex    bl                = {-0.5f, -0.5f, 0, 0, 0};
    Vertex    br                = {0.5f, -0.5f, 0, 1, 0};
    Vertex    tr                = {0.5f, 0.5f, 0, 1, 1};
    Vertex    tl                = {-0.5f, 0.5f, 0, 0, 1};
    Vertex    square_vertices[] = {bl, br, tr, bl, tr, tl};
    gfx::Mesh square = platform_.get_graphics_backend().load_mesh(square_vertices, std::size(square_vertices));

    Model dragon = load_model(*this, "../data/models/dragon.obj");

    // Add dragons
    i32 num_meshes = 20;
    for (i32 i = 0; i < num_meshes; ++i) {
        const auto entity = registry_.create();
        registry_.emplace<Transformation>(entity, Transformation());
        registry_.emplace<Model>(entity, dragon);
    }

    while (running_) {
        platform_.update();
        f32 dt   = platform_.get_delta_time();
        f32 time = (f32)glfwGetTime();

        // player camera

        f32 move_speed = 1.0f * dt;
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
            camera.add_position(glm::vec3(0, move_speed, 0));
        }
        if (platform_.is_key_down(GLFW_KEY_LEFT_CONTROL)) {
            camera.add_position(glm::vec3(0, -move_speed, 0));
        }
        if (platform_.is_mouse_grabbed()) {
            camera.add_pitch(platform_.get_mouse_delta().y * 0.001f);
            camera.add_yaw(platform_.get_mouse_delta().x * 0.001f);
        }
        if (platform_.is_key_pressed(GLFW_KEY_TAB)) {
            platform_.set_mouse_grabbed(!platform_.is_mouse_grabbed());
        }
        renderer_.set_camera(camera);

        // render

        i32 i = 0;
        for (auto [entity, transformation, model] : registry_.view<Transformation, const Model>().each()) {
            const f32 t        = (0.25f * time + float(i) / float(num_meshes - 1)) * glm::two_pi<f32>();
            const f32 distance = 0.75f;
            const f32 scale    = std::abs(std::sin(t));

            if (i == 0) {
                transformation.position = glm::vec3(0, 0, -1);
            } else {
                transformation.position =
                    distance * glm::vec3(std::cos(t), std::sin(t) * 0.5f, -0.5f * std::abs(std::cos(t)) - 1);
                transformation.scale = glm::vec3(scale);
            }

            model.add_to_scene(renderer_, transformation.get_matrix());
            ++i;
        }
        renderer_.render();
    }
}

} // namespace rune
