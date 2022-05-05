#ifndef RUNE_CORE_H
#define RUNE_CORE_H

#include "config.h"
#include "logger.h"
#include "platform.h"
#include "renderer.h"

#include <entt/entt.hpp>

#include <optional>

namespace rune {

class Core final {
  public:
    static Core& get() {
        static Core core;
        return core;
    }

    void run();

    void stop() {
        running_ = false;
    }

    Logger& get_logger() {
        return logger_;
    }

    Config& get_config() {
        return config_;
    }

    Platform& get_platform() {
        return platform_;
    }

  private:
    Core();

    Logger   logger_;
    Config   config_;
    Platform platform_;
    Renderer renderer_;

    entt::registry registry_;

    bool running_ = true;
};

} // namespace rune

#endif // RUNE_CORE_H
