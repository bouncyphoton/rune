#ifndef RUNE_RENDERER_H
#define RUNE_RENDERER_H

namespace rune {

class Core;
class GraphicsBackend;

// graphics frontend

class Renderer {
  public:
    explicit Renderer(Core& core);

    void render();

  private:
    Core&            core_;
    GraphicsBackend& gfx_;
};

} // namespace rune

#endif // RUNE_RENDERER_H
