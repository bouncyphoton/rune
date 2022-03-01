#ifndef RUNE_RENDERER_H
#define RUNE_RENDERER_H

#include "gfx/camera.h"
#include <glm/glm.hpp>
#include <vector>

namespace rune {

class Core;

namespace gfx {

class GraphicsBackend;

}

struct RenderObject {
    glm::mat4 model_matrix;
};

// graphics frontend

class Renderer {
  public:
    explicit Renderer(Core& core);

    /**
     * Add a render object to be rendered this frame
     * @param robj Render object data for rendering
     */
    void add_to_frame(const RenderObject& robj);

    /**
     * Set the camera for the next render
     * @param camera Camera
     */
    void set_camera(const Camera& camera) {
        camera_ = camera;
    }

    /**
     * Render a frame with all the render objects added since the last call to render
     */
    void render();

  private:
    void process_object_data();
    void reset_frame();

    Core&                 core_;
    gfx::GraphicsBackend& gfx_;

    Camera camera_;

    // temporary unordered objects to render
    std::vector<RenderObject> render_objects_;
};

} // namespace rune

#endif // RUNE_RENDERER_H
