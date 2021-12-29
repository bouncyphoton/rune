#ifndef RUNE_CONFIG_H
#define RUNE_CONFIG_H

#include "types.h"

namespace rune {

class Core;

class Config {
  public:
    explicit Config(Core& core);

    [[nodiscard]] u32 get_window_width() const {
        return window_width_;
    }

    [[nodiscard]] u32 get_window_height() const {
        return window_height_;
    }

  private:
    u32 window_width_  = 800;
    u32 window_height_ = 600;
};

} // namespace rune

#endif // RUNE_CONFIG_H
