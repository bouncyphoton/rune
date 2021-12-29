#include "core.h"

namespace rune {

Core::Core() : config_(*this), platform_(*this), renderer_(*this) {
    logger_.info("operating system: %", consts::os_name);
    logger_.info("is release build: %", consts::is_release);
}

void Core::run() {
    while (running_) {
        platform_.update();

        // do updates here

        renderer_.render();
    }
}

} // namespace rune
