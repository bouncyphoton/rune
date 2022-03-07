#include "core.h"

#include <GLFW/glfw3.h>

namespace rune {

Core::Core() : config_(*this), platform_(*this), renderer_(*this) {
    logger_.info("operating system: %", consts::os_name);
    logger_.info("is release build: %", consts::is_release);
}

void Core::run() {
    f32    aspect_ratio = (f32)config_.get_window_width() / (f32)config_.get_window_height();
    Camera camera(glm::half_pi<f32>(), aspect_ratio, 0.01f, 100.0f, glm::vec3(0), glm::vec3(0, 0, -1));

    // clockwise upside down
    Vertex    triangle_vertices[] = {Vertex{0.5f, 0.5f, 0, 1, 1},
                                  Vertex{0, -0.5f, 0, 0.5f, 0},
                                  Vertex{-0.5f, 0.5f, 0, 0, 1}};
    gfx::Mesh triangle = platform_.get_graphics_backend().load_mesh(triangle_vertices, std::size(triangle_vertices));

    Vertex    bl                = {-0.5f, -0.5f, 0, 0, 0};
    Vertex    br                = {0.5f, -0.5f, 0, 1, 0};
    Vertex    tr                = {0.5f, 0.5f, 0, 1, 1};
    Vertex    tl                = {-0.5f, 0.5f, 0, 0, 1};
    Vertex    square_vertices[] = {tr, br, bl, tr, bl, tl};
    gfx::Mesh square = platform_.get_graphics_backend().load_mesh(square_vertices, std::size(square_vertices));

    while (running_) {
        platform_.update();

        // do updates here
        f32 time = (f32)glfwGetTime();
        camera.set_position(glm::vec3(std::sin(time), 0, 0));
        renderer_.set_camera(camera);

        i32 num_meshes = 20;
        for (i32 i = 0; i < num_meshes; ++i) {
            RenderObject obj;
            //obj.mesh = (i < num_meshes / 2) ? triangle : square;
            obj.mesh = (i % 2 == 0) ? triangle : square;

            /*
            obj.model_matrix = ;
            obj.mesh_id      = ;
            obj.material_id  = ;
            */

            if (i == 0) {
                obj.model_matrix = glm::translate(glm::mat4(1), glm::vec3(0, 0, -1));
            } else {
                const f32 t        = (0.25f * time + float(i) / float(num_meshes - 1)) * glm::two_pi<f32>();
                const f32 distance = 0.75f;
                const f32 scale    = 0.25f;
                glm::vec3 pos      = distance * glm::vec3(std::cos(t), std::sin(t), -1);

                obj.model_matrix = glm::translate(glm::mat4(1), pos) * glm::scale(glm::mat4(1), glm::vec3(scale));
            }

            renderer_.add_to_frame(obj);
        }

        renderer_.render();
    }
}

} // namespace rune
