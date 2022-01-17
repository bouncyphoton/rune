#ifndef RUNE_GRAPHICS_BACKEND_H
#define RUNE_GRAPHICS_BACKEND_H

#include "gfx/render_pass.h"
#include "types.h"

#include <functional>
#include <stack>
#include <vk_mem_alloc.h>
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

    Buffer get_unified_vertex_buffer() {
        return unified_vertex_buffer_;
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

    // descriptor sets are not dynamic, use for per-frame things

    VkDescriptorSet get_descriptor_set(VkDescriptorSetLayout layout);
    void            update_descriptor_sets(const std::vector<VkWriteDescriptorSet>& writes);

  private:
    // TODO: config option?
    static constexpr u32 NUM_FRAMES_IN_FLIGHT = 2;
    static constexpr u32 MAX_UNIQUE_VERTICES  = 1000;

    struct DescriptorSetCache {
        [[nodiscard]] bool empty() const {
            return available_.empty();
        }

        void add_in_use(VkDescriptorSet set) {
            in_use_.push(set);
        }

        VkDescriptorSet get_for_use() {
            if (empty()) {
                return VK_NULL_HANDLE;
            }

            VkDescriptorSet set = available_.top();
            available_.pop();
            in_use_.push(set);
            return set;
        }

        void reset() {
            while (!in_use_.empty()) {
                available_.push(in_use_.top());
                in_use_.pop();
            }
        }

      private:
        std::stack<VkDescriptorSet> in_use_;
        std::stack<VkDescriptorSet> available_;
    };

    struct PerFrame {
        VkCommandBuffer command_buffer_;
        VkSemaphore     image_available_;
        VkSemaphore     render_finished_;
        VkFence         in_flight_;

        std::unordered_map<VkDescriptorSetLayout, DescriptorSetCache> descriptor_set_caches_;

        DescriptorSetCache& get_descriptor_set_cache(VkDescriptorSetLayout layout) {
            return descriptor_set_caches_[layout];
        }
    };

    void choose_physical_device();
    void create_logical_device();
    void create_swapchain();

    void one_time_submit(VkQueue queue, const std::function<void(VkCommandBuffer)>& cmd_recording_func);

    enum class BufferDestroyPolicy
    {
        MANUAL_DESTROY,   // you must call destroy_buffer
        AUTOMATIC_DESTROY // destroy_buffer will be called at application end
    };

    Buffer create_buffer_gpu(VkDeviceSize size, VkBufferUsageFlags buffer_usage, BufferDestroyPolicy policy);
    void   copy_to_buffer(void* src_data, VkDeviceSize src_size, const Buffer& dst_buffer, VkDeviceSize offset);
    void   destroy_buffer(const Buffer& buffer);

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

    VmaAllocator allocator_ = VK_NULL_HANDLE;

    // need a command pool per-thread
    VkCommandPool command_pool_ = VK_NULL_HANDLE;

    VkDescriptorPool descriptor_pool_ = VK_NULL_HANDLE;

    PerFrame frames_[NUM_FRAMES_IN_FLIGHT] = {};
    u32      current_frame_                = 0;
    u32      swap_image_index_             = 0;

    std::unordered_map<VkRenderPass, std::vector<VkFramebuffer>> framebuffers_;

    Buffer unified_vertex_buffer_;
};

} // namespace rune::gfx

#endif // RUNE_GRAPHICS_BACKEND_H
