#include "core.h"

#include <GLFW/glfw3.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

namespace rune {

struct Model {
    using MeshData = std::vector<Vertex>;

    explicit Model(Core& core, const std::vector<MeshData>& meshes) : core_(core) {
        for (const auto& mesh_data : meshes) {
            meshes_.emplace_back(core_.get_platform().get_graphics_backend().load_mesh(mesh_data));
        }
    }

    void add_to_scene(Renderer& renderer, glm::mat4 transformation) {
        RenderObject robj = {};
        robj.model_matrix = transformation;

        for (const auto& mesh : meshes_) {
            robj.mesh = mesh;
            renderer.add_to_frame(robj);
        }
    }

  private:
    Core&                  core_;
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

    std::vector<std::vector<Vertex>> meshes;

    // go over shapes
    for (u32 s = 0; s < shapes.size(); ++s) {
        // go over faces
        u32 index_offset = 0;
        for (u32 f = 0; f < shapes[s].mesh.num_face_vertices.size(); ++f) {
            u32 fv = shapes[s].mesh.num_face_vertices[f];
            rune_assert(core, fv == 3); // must be triangle

            i32 mat_id = shapes[s].mesh.material_ids[f] + 1;
            if (mat_id >= meshes.size()) {
                meshes.resize(mat_id + 1);
            }
            auto& mesh = meshes[mat_id];

            // loop over vertices
            for (u32 v = 0; v < fv; ++v) {
                tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];

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

            // also material here
            // shapes[s].mesh.material_ids[f];
        }
    }

    core.get_logger().info("loaded % meshes for model '%'", meshes.size(), path);

    return Model(core, meshes);
}

Core::Core() : config_(*this), platform_(*this), renderer_(*this) {
    logger_.info("operating system: %", consts::os_name);
    logger_.info("is release build: %", consts::is_release);
}

void Core::run() {
    f32    aspect_ratio = (f32)config_.get_window_width() / (f32)config_.get_window_height();
    Camera camera(glm::half_pi<f32>(), aspect_ratio, 0.01f, 100.0f, glm::vec3(0), glm::vec3(0, 0, -1));

    // counter clockwise winding order

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

    while (running_) {
        platform_.update();

        // do updates here
        f32 time = (f32)glfwGetTime();
        camera.set_position(glm::vec3(std::sin(time), 0, 0));
        renderer_.set_camera(camera);

        i32 num_meshes = 20;
        for (i32 i = 0; i < num_meshes; ++i) {
            // obj.mesh = (i < num_meshes / 2) ? triangle : square;
            //obj.mesh = (i % 2 == 0) ? triangle : square;

            /*
            obj.model_matrix = ;
            obj.mesh_id      = ;
            obj.material_id  = ;
            */

            glm::mat4 transformation = glm::mat4(1);

            if (i == 0) {
                transformation = glm::translate(glm::mat4(1), glm::vec3(0, 0, -1));
            } else {
                const f32 t        = (0.25f * time + float(i) / float(num_meshes - 1)) * glm::two_pi<f32>();
                const f32 distance = 0.75f;
                const f32 scale    = std::abs(std::sin(t));
                glm::vec3 pos =
                    distance * glm::vec3(std::cos(t), std::sin(t) * 0.5f, -0.5f * std::abs(std::cos(t)) - 1);

                transformation = glm::translate(glm::mat4(1), pos) * glm::scale(glm::mat4(1), glm::vec3(scale));
            }

            dragon.add_to_scene(renderer_, transformation);
        }

        renderer_.render();
    }
}

} // namespace rune
