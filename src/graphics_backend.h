#ifndef RUNE_GRAPHICS_BACKEND_H
#define RUNE_GRAPHICS_BACKEND_H

#include "types.h"

#include <functional>
#include <spirv_reflect.h>
#include <stack>
#include <vulkan/vulkan.h>

typedef struct GLFWwindow GLFWwindow;

namespace rune {

class Core;

struct GraphicsPassDesc {
    // 0,0 will default to full res
    VkRect2D render_area = {0, 0};

    // temp shader paths

    const char* vert_shader_path = nullptr;
    const char* frag_shader_path = nullptr;
};

// TODO
struct GraphicsPass {
    VkRenderPass     render_pass;
    VkRect2D         render_area;
    VkPipelineLayout pipeline_layout;
};

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

    GraphicsPass create_pass(GraphicsPassDesc desc);

    void start_pass(const GraphicsPass& pass);
    void end_pass();

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

    struct ShaderData {
        VkShaderModule    shader_module;
        std::vector<char> code;
    };

    ShaderData create_shader_module(const char* shader_path);

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

} // namespace rune

#endif // RUNE_GRAPHICS_BACKEND_H
