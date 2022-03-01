#include "core.h"
#include "math/constants.h"

#include <GLFW/glfw3.h>

namespace rune {

Core::Core() : config_(*this), platform_(*this), renderer_(*this) {
    logger_.info("operating system: %", consts::os_name);
    logger_.info("is release build: %", consts::is_release);
}

void Core::run() {
    while (running_) {
        platform_.update();

        f32 time = (f32)glfwGetTime();

        // do updates here
        i32 num_meshes = 20;
        for (i32 i = 0; i < num_meshes; ++i) {
            RenderObject obj;
            /*
            obj.model_matrix = ;
            obj.mesh_id      = ;
            obj.material_id  = ;
            */

            if (i == 0) {
                obj.model_matrix = Mat4x4f(1);
            } else {
                const f32 t         = (0.25f * time - float(i) / float(num_meshes - 1)) * math::TWO_PI;
                const f32 distance  = 0.75f;
                const f32 scale     = 0.25f;
                obj.model_matrix    = Mat4x4f(scale);
                obj.model_matrix[3] = Vec4f(distance * std::sin(t), distance * std::cos(t), 0, 1);
            }

            renderer_.add_to_frame(obj);
        }

        renderer_.render();
    }
}

} // namespace rune
