#include "platform.h"

#include "core.h"
#include "utils.h"

#include <GLFW/glfw3.h>

namespace rune {

enum class KeyState {
    UP, PRESSED, DOWN
};
static std::unordered_map<int, KeyState> g_key_states;

Platform::Platform(Core& core) : core_(core) {
    Config& conf = core.get_config();

    rune_assert(core_, conf.get_window_width() > 0);
    rune_assert(core_, conf.get_window_height() > 0);
    rune_assert(core_, glfwInit() == GLFW_TRUE);

    rune_assert(core_, glfwVulkanSupported());

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    window_ = glfwCreateWindow(static_cast<i32>(conf.get_window_width()),
                               static_cast<i32>(conf.get_window_height()),
                               "rune",
                               nullptr,
                               nullptr);
    rune_assert(core_, window_ != nullptr);

    glfwSetKeyCallback(window_, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
        switch (action) {
        case GLFW_PRESS:
            if (g_key_states[key] == KeyState::UP) {
                g_key_states[key] = KeyState::PRESSED;
            }
            break;
        case GLFW_RELEASE:
            g_key_states[key] = KeyState::UP;
            break;
        }
    });

    graphics_.emplace(core_, window_);
}

Platform::~Platform() {
    glfwDestroyWindow(window_);
    glfwTerminate();
}

void Platform::update() {
    for (auto& [key, state] : g_key_states) {
        if (state == KeyState::PRESSED) {
            state = KeyState::DOWN;
        }
    }

    glfwPollEvents();

    if (glfwWindowShouldClose(window_)) {
        core_.stop();
    }

    calculate_delta_time();
    update_input();
}

bool Platform::is_key_down(int key) const {
    return g_key_states[key] != KeyState::UP;
}

bool Platform::is_key_pressed(int key) const {
    return g_key_states[key] == KeyState::PRESSED;
}

bool Platform::is_mouse_grabbed() const {
    int input_mode = glfwGetInputMode(window_, GLFW_CURSOR);
    return (input_mode == GLFW_CURSOR_DISABLED);
}

void Platform::set_mouse_grabbed(bool is_grabbed) {
    int input_mode = (is_grabbed ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    glfwSetInputMode(window_, GLFW_CURSOR, input_mode);
}

glm::vec2 Platform::get_mouse_delta() const {
    return mouse_pos_ - prev_mouse_pos_;
}

void Platform::calculate_delta_time() {
    auto now     = std::chrono::steady_clock::now();
    u32  diff_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now - last_update_).count();
    last_update_ = now;
    delta_time_  = (f32)(diff_ns / (f64)std::nano::den);
}

void Platform::update_input() {
    double x, y;
    glfwGetCursorPos(window_, &x, &y);

    int width, height;
    glfwGetWindowSize(window_, &width, &height);

    y = height - y;

    prev_mouse_pos_ = mouse_pos_;
    mouse_pos_      = glm::vec2(x, y);
}

} // namespace rune
