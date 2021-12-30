#ifndef RUNE_GRAPHICS_BACKEND_H
#define RUNE_GRAPHICS_BACKEND_H

#include "gfx/render_pass.h"
#include "types.h"

#include <functional>
#include <stack>
#include <vulkan/vulkan.h>

typedef struct GLFWwindow GLFWwindow;

namespace rune {

class Core;

}

namespace rune::gfx {

class GraphicsBackend {
  public:
    explicit GraphicsBackend(Core& core, GLFWwindow* window);
    ~GraphicsBackend();

    void begin_frame();
    void end_frame();

    // get the command buffer for the current frame
    VkCommandBuffer get_command_buffer() {
        return get_current_frame().command_buffer_;
    }

    // temp
    VkRenderPass          create_render_pass();
    void                  create_framebuffers(VkRenderPass render_pass, VkRect2D render_area);
    VkFramebuffer         get_framebuffer(VkRenderPass render_pass);
    VkDescriptorSetLayout create_descriptor_set_layout(const VkDescriptorSetLayoutCreateInfo& info);
    VkPipelineLayout      create_pipeline_layout(const VkPipelineLayoutCreateInfo& pipeline_layout_info);
    VkPipeline            create_graphics_pipeline(const std::vector<ShaderInfo>& shaders,
                                                   VkPipelineLayout               pipeline_layout,
                                                   VkRenderPass                   render_pass);

  private:
    // TODO: config option?
    static constexpr u32 NUM_FRAMES_IN_FLIGHT = 2;

    struct PerFrame {
        VkCommandBuffer command_buffer_;
        VkSemaphore     image_available_;
        VkSemaphore     render_finished_;
        VkFence         in_flight_;
    };

    void choose_physical_device();
    void create_logical_device();
    void create_swapchain();

    PerFrame& get_current_frame() {
        return frames_[current_frame_];
    }

    Core&                             core_;
    std::stack<std::function<void()>> cleanup_;

    VkInstance       instance_              = VK_NULL_HANDLE;
    VkSurfaceKHR     surface_               = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device_       = VK_NULL_HANDLE;
    VkDevice         device_                = VK_NULL_HANDLE;
    u32              graphics_family_index_ = 0;
    u32              compute_family_index_  = 0;
    u32              present_family_index_  = 0;
    VkQueue          graphics_queue_        = VK_NULL_HANDLE;
    VkQueue          compute_queue_         = VK_NULL_HANDLE;
    VkQueue          present_queue_         = VK_NULL_HANDLE;

    VkSwapchainKHR           swapchain_        = VK_NULL_HANDLE;
    VkExtent2D               swapchain_extent_ = {};
    VkSurfaceFormatKHR       swapchain_format_ = {};
    std::vector<VkImage>     swapchain_images_;
    std::vector<VkImageView> swapchain_image_views_;

    // need a command pool per-thread
    VkCommandPool command_pool_ = VK_NULL_HANDLE;

    PerFrame frames_[NUM_FRAMES_IN_FLIGHT] = {};
    u32      current_frame_                = 0;
    u32      swap_image_index_             = 0;

    std::unordered_map<VkRenderPass, std::vector<VkFramebuffer>> framebuffers_;
};

} // namespace rune::gfx

#endif // RUNE_GRAPHICS_BACKEND_H
