#ifndef RUNE_RENDERER_H
#define RUNE_RENDERER_H

#include "math/matrix.h"

#include <vector>

namespace rune {

class Core;

namespace gfx {

class GraphicsBackend;

}

struct RenderObject {
    Mat4x4f model_matrix;
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
     * Render a frame with all the render objects added since the last call to render
     */
    void render();

  private:
    void process_object_data();
    void reset_frame();

    Core&                 core_;
    gfx::GraphicsBackend& gfx_;

    // temporary unordered objects to render
    std::vector<RenderObject> render_objects_;
};

} // namespace rune

#endif // RUNE_RENDERER_H
