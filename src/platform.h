#ifndef RUNE_PLATFORM_H
#define RUNE_PLATFORM_H

#include "graphics_backend.h"

#include <optional>

typedef struct GLFWwindow GLFWwindow;

namespace rune {

class Core;

class Platform {
  public:
    explicit Platform(Core& core);
    ~Platform();

    void update();

    GraphicsBackend& get_graphics_backend() {
        return *graphics_;
    }

  private:
    Core&                          core_;
    GLFWwindow*                    window_;
    std::optional<GraphicsBackend> graphics_;
};

} // namespace rune

#endif // RUNE_PLATFORM_H
