#include "platform.h"

#include "core.h"
#include "utils.h"

#include <GLFW/glfw3.h>

namespace rune {

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

    graphics_.emplace(core_, window_);
}

Platform::~Platform() {
    glfwDestroyWindow(window_);
    glfwTerminate();
}

void Platform::update() {
    glfwPollEvents();

    if (glfwWindowShouldClose(window_)) {
        core_.stop();
    }
}

} // namespace rune
