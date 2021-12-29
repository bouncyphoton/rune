#ifndef RUNE_CORE_H
#define RUNE_CORE_H

#include "config.h"
#include "logger.h"
#include "platform.h"
#include "renderer.h"

#include <optional>

namespace rune {

class Core final {
  public:
    Core();

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
    Logger   logger_;
    Config   config_;
    Platform platform_;
    Renderer renderer_;

    bool running_ = true;
};

} // namespace rune

#endif // RUNE_CORE_H
