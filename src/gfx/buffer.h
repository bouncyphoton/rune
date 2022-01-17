#ifndef RUNE_BUFFER_H
#define RUNE_BUFFER_H

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace rune::gfx {

struct Buffer {
    VkBuffer     buffer = VK_NULL_HANDLE;
    VkDeviceSize range  = 0;

    VmaAllocation     allocation      = VK_NULL_HANDLE;
    VmaAllocationInfo allocation_info = {};
};

} // namespace rune::gfx

#endif // RUNE_BUFFER_H
