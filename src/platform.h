#ifndef RUNE_PLATFORM_H
#define RUNE_PLATFORM_H

#include "gfx/graphics_backend.h"

#include <chrono>
#include <optional>

typedef struct GLFWwindow GLFWwindow;

namespace rune {

class Core;

class Platform {
  public:
    explicit Platform(Core& core);
    ~Platform();

    void update();

    gfx::GraphicsBackend& get_graphics_backend() {
        return *graphics_;
    }

    [[nodiscard]] f32 get_delta_time() const {
        return delta_time_;
    }

    [[nodiscard]] bool is_key_down(int key) const;

    [[nodiscard]] bool is_key_pressed(int key) const;

    bool is_mouse_grabbed() const;

    void set_mouse_grabbed(bool is_grabbed);

    glm::vec2 get_mouse_delta() const;

  private:
    void calculate_delta_time();
    void update_input();

    Core&                          core_;
    GLFWwindow*                    window_;
    std::optional<gfx::GraphicsBackend> graphics_;

    std::chrono::steady_clock::time_point last_update_;
    f32 delta_time_;
    glm::vec2 prev_mouse_pos_;
    glm::vec2 mouse_pos_;
};

} // namespace rune

#endif // RUNE_PLATFORM_H
