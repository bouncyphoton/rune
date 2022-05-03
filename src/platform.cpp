#include "platform.h"

#include "core.h"
#include "utils.h"

#include <GLFW/glfw3.h>

namespace rune {

enum class KeyState
{
    UP,
    PRESSED,
    DOWN,
    RELEASED
};
static std::unordered_map<int, KeyState> g_key_states;
static void                              process_key(int key, int action) {
    switch (action) {
    case GLFW_PRESS:
        if (g_key_states[key] == KeyState::UP) {
            g_key_states[key] = KeyState::PRESSED;
        }
        break;
    case GLFW_RELEASE:
        if (g_key_states[key] != KeyState::UP) {
            g_key_states[key] = KeyState::RELEASED;
        }
        break;
    }
}
static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {}
static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {}

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
        //
        process_key(key, action);
    });
    glfwSetMouseButtonCallback(window_, [](GLFWwindow* window, int key, int action, int mods) {
        //
        process_key(key, action);
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
        } else if (state == KeyState::RELEASED) {
            state = KeyState::UP;
        }
    }

    glfwPollEvents();

    if (glfwWindowShouldClose(window_)) {
        core_.stop();
    }

    calculate_delta_time();
    update_mouse();
}

bool Platform::is_key_down(int key) const {
    KeyState state = g_key_states[key];
    return state == KeyState::DOWN || state == KeyState::PRESSED;
}

bool Platform::is_key_pressed(int key) const {
    return g_key_states[key] == KeyState::PRESSED;
}

bool Platform::is_key_released(int key) const {
    return g_key_states[key] == KeyState::RELEASED;
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

void Platform::update_mouse() {
    double x, y;
    glfwGetCursorPos(window_, &x, &y);

    int width, height;
    glfwGetWindowSize(window_, &width, &height);

    y = height - y;

    prev_mouse_pos_ = mouse_pos_;
    mouse_pos_      = glm::vec2(x, y);
}

} // namespace rune
