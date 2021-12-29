#include "config.h"

#include "core.h"

namespace rune {

Config::Config(Core& core) {
    core.get_logger().info("using default values for config");
}

} // namespace rune
