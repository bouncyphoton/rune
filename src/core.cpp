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

    while (running_) {
        platform_.update();

        // do updates here
        f32 time = (f32)glfwGetTime();
        camera.set_position(glm::vec3(std::sin(time), 0, 0));
        renderer_.set_camera(camera);

        i32 num_meshes = 20;
        for (i32 i = 0; i < num_meshes; ++i) {
            RenderObject obj;
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
