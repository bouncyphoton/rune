#ifndef RUNE_TEXTURE_H
#define RUNE_TEXTURE_H

#include <vulkan/vulkan.h>

namespace rune::gfx {

class Texture {
  public:
    Texture(VkImage image, VkImageView view, VkFormat format, VkImageLayout layout)
        : image_(image), image_view_(view), format_(format), layout_(layout) {}

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

  private:
    VkImage       image_;
    VkImageView   image_view_;
    VkFormat      format_;
    VkImageLayout layout_;
};

} // namespace rune::gfx

#endif // RUNE_TEXTURE_H
