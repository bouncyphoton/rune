#ifndef RUNE_TEXTURE_H
#define RUNE_TEXTURE_H

#include "types.h"

#include <vulkan/vulkan.h>

namespace rune::gfx {

class Texture {
  public:
    Texture(VkImage image, VkImageView view, VkFormat format, VkImageLayout layout, VkExtent3D extent)
        : image_(image), image_view_(view), format_(format), layout_(layout), width_(extent.width),
          height_(extent.height), depth_(extent.depth) {}

    [[nodiscard]] VkImage get_image() const {
        return image_;
    }

    [[nodiscard]] VkImageView get_image_view() const {
        return image_view_;
    }

    [[nodiscard]] VkFormat get_format() const {
        return format_;
    }

    [[nodiscard]] VkImageLayout get_image_layout() const {
        return layout_;
    }

    [[nodiscard]] u32 get_width() const {
        return width_;
    }

    [[nodiscard]] u32 get_height() const {
        return height_;
    }

    [[nodiscard]] u32 get_depth() const {
        return depth_;
    }

  private:
    VkImage       image_;
    VkImageView   image_view_;
    VkFormat      format_;
    VkImageLayout layout_;
    u32           width_;
    u32           height_;
    u32           depth_;
};

} // namespace rune::gfx

#endif // RUNE_TEXTURE_H
