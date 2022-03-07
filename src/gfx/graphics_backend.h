#ifndef RUNE_GRAPHICS_BACKEND_H
#define RUNE_GRAPHICS_BACKEND_H

#include "gfx/render_pass.h"
#include "types.h"
#include "vertex.h"

#include <functional>
#include <glm/glm.hpp>
#include <stack>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

typedef struct GLFWwindow GLFWwindow;

namespace rune {

class Core;

}

namespace rune::gfx {

struct ObjectData {
    glm::mat4 model_matrix;
};

struct Mesh {
    Mesh() : Mesh(0, 0) {}
    Mesh(u32 first_vertex, u32 num_vertices) : first_vertex_(first_vertex), num_vertices_(num_vertices) {}

    [[nodiscard]] u32 get_first_vertex() const {
        return first_vertex_;
    }

    [[nodiscard]] u32 get_num_vertices() const {
        return num_vertices_;
    }

    [[nodiscard]] u64 get_id() const {
        return id_;
    }

    bool operator==(const Mesh& rhs) const {
        return first_vertex_ == rhs.first_vertex_ && num_vertices_ == rhs.num_vertices_;
    }
    bool operator!=(const Mesh& rhs) const {
        return !(rhs == *this);
    }

  private:
    union {
        struct {
            u32 first_vertex_;
            u32 num_vertices_;
        };
        u64 id_;
    };
};

struct MeshBatch {
    u32  first_object_idx = 0;
    u32  num_objects      = 0;
    Mesh mesh;
};

struct BatchGroup {
    u32 first_batch = 0;
    u32 num_batches = 0;
};

class GraphicsBackend {
  public:
    explicit GraphicsBackend(Core& core, GLFWwindow* window);
    ~GraphicsBackend();

    /**
     * Waits for frame from last time around to be ready, gets next image, starts command buffer
     */
    void begin_frame();

    /**
     * Submits command buffer, queues for presentation, increments current frame
     */
    void end_frame();

    /**
     * Get the command buffer for the current frame
     * @return Current frame's command buffer
     */
    VkCommandBuffer get_command_buffer() {
        // TODO: multithreading?

        return get_current_frame().command_buffer_;
    }

    Buffer get_unified_vertex_buffer() {
        return unified_vertex_buffer_;
    }

    Buffer get_object_data_buffer() {
        return get_current_frame().object_data_;
    }

    void update_object_data(const std::vector<ObjectData>& data) {
        update_object_data(data.data(), data.size());
    }

    void update_object_data(const ObjectData* data, u32 num_objects);

    Mesh load_mesh(const std::vector<Vertex>& vertices) {
        return load_mesh(vertices.data(), vertices.size());
    }

    Mesh load_mesh(const Vertex* data, u32 num_vertices);

    BatchGroup add_batches(const std::vector<gfx::MeshBatch>& batches);

    void draw_batch_group(VkCommandBuffer cmd, const BatchGroup& group);

    // temp
    VkRenderPass          create_render_pass();
    void                  create_framebuffers(VkRenderPass render_pass, VkRect2D render_area);
    VkFramebuffer         get_framebuffer(VkRenderPass render_pass);
    VkDescriptorSetLayout create_descriptor_set_layout(const VkDescriptorSetLayoutCreateInfo& info);
    VkPipelineLayout      create_pipeline_layout(const VkPipelineLayoutCreateInfo& pipeline_layout_info);
    VkPipeline            create_graphics_pipeline(const std::vector<ShaderInfo>& shaders,
                                                   VkPipelineLayout               pipeline_layout,
                                                   VkRenderPass                   render_pass);

    /**
     * Get a descriptor set with the given layout, if none are available, allocate a new one
     * @note Descriptor sets are not dynamic, it's recommended that you use them for per-frame things
     * @param layout The layout the descriptor set should match
     * @return The descriptor set
     */
    VkDescriptorSet get_descriptor_set(VkDescriptorSetLayout layout);

    /**
     * Update descriptor sets with a vector of writes.
     * Just a simple wrapper around vkUpdateDescriptorSets
     * @param writes A vector of VkWriteDescriptorSet
     */
    void update_descriptor_sets(const std::vector<VkWriteDescriptorSet>& writes);

  private:
    // TODO: config option?
    static constexpr u32 NUM_FRAMES_IN_FLIGHT = 2;
    static constexpr u32 MAX_UNIQUE_VERTICES  = 1000;
    static constexpr u32 MAX_OBJECTS          = 1000;
    static constexpr u32 MAX_DRAWS            = 1000;

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
        Buffer          object_data_;
        Buffer          draw_data_; // holds VkDrawIndirectCommands
        u32             num_draws_; // aka num_batches

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
    void   copy_to_buffer(const void* src_data, VkDeviceSize src_size, const Buffer& dst_buffer, VkDeviceSize offset);
    void   destroy_buffer(const Buffer& buffer);

    PerFrame& get_current_frame() {
        return frames_[current_frame_];
    }

    Core&                             core_;
    std::stack<std::function<void()>> cleanup_;

    VkInstance               instance_              = VK_NULL_HANDLE;
    VkSurfaceKHR             surface_               = VK_NULL_HANDLE;
    VkPhysicalDevice         physical_device_       = VK_NULL_HANDLE;
    VkPhysicalDeviceFeatures device_features_       = {};
    VkDevice                 device_                = VK_NULL_HANDLE;
    u32                      graphics_family_index_ = 0;
    u32                      compute_family_index_  = 0;
    u32                      present_family_index_  = 0;
    VkQueue                  graphics_queue_        = VK_NULL_HANDLE;
    VkQueue                  compute_queue_         = VK_NULL_HANDLE;
    VkQueue                  present_queue_         = VK_NULL_HANDLE;

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

    // unified buffers
    Buffer unified_vertex_buffer_;
    u32    num_vertices_in_buffer_ = 0;
};

} // namespace rune::gfx

#endif // RUNE_GRAPHICS_BACKEND_H
