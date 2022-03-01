#ifndef RUNE_RENDER_PASS_H
#define RUNE_RENDER_PASS_H

#include "buffer.h"
#include "consts.h"
#include "types.h"

#include <functional>
#include <string>
#include <vulkan/vulkan.h>

namespace rune {
class Core;
}

namespace rune::gfx {

class GraphicsBackend;

/**
 * Holds info relating to a shader
 */
struct ShaderInfo {
    VkShaderStageFlagBits stage;
    const char*           path;
};

/**
 * Holds data relating to writing to descriptors
 */
struct DescriptorWrites {
    struct Write {
        enum class WriteDataType
        {
            INVALID,
            BUFFER,
            IMAGE
        };

        VkDescriptorType descriptor_type;
        WriteDataType    write_type;
        union {
            VkDescriptorBufferInfo buffer_info;
            VkDescriptorImageInfo  image_info;
        } data;
    };

    const std::unordered_map<std::string, Write>& get_write_data() const {
        return write_data_;
    }

    void set_buffer(const std::string& name, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range) {
        Write& write          = write_data_[name];
        write.descriptor_type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write.write_type      = Write::WriteDataType::BUFFER;

        write.data.buffer_info.buffer = buffer;
        write.data.buffer_info.offset = offset;
        write.data.buffer_info.range  = range;
    }

    void set_buffer(const std::string& name, const Buffer& buffer, VkDeviceSize offset = 0) {
        set_buffer(name, buffer.buffer, offset, buffer.range);
    }

  private:
    // variable name -> write info
    std::unordered_map<std::string, Write> write_data_;
};

/**
 * An interface for render passes to be built from
 */
class RenderPass {
  public:
    explicit RenderPass(Core& core, GraphicsBackend& gfx, const std::vector<ShaderInfo>& shaders);
    virtual ~RenderPass() = default;

    RenderPass(const RenderPass&) = delete;
    RenderPass& operator=(const RenderPass&) = delete;

    /**
     * Bind the render pass and run func to fill the command buffer for this pass
     * @param cmd The command buffer to be used
     * @param func The function to run
     */
    virtual void run(VkCommandBuffer cmd, const std::function<void(VkCommandBuffer)>& func) = 0;

    /**
     * Set push constants for this render pass. Should be called within the function that's passed to run
     * @tparam T The type of data to set the push constant to
     * @param cmd The command buffer to write to
     * @param shader_stage The shader stage that the push constants belong to
     * @param data The data to set in the push constant
     * @param offset The offset in bytes into the push constant
     */
    template <typename T>
    void set_push_constants(VkCommandBuffer cmd, VkShaderStageFlagBits shader_stage, const T& data, u32 offset = 0) {
        set_push_constants(cmd, shader_stage, &data, sizeof(data), offset);
    }

    /**
     * Set push constants for this render pass. Should be called within the function that's passed to run.
     * Prefer to use the templated version as it calculates the size for you.
     * @param cmd The command buffer to write to
     * @param shader_stage The shader stage that the push constants belong to
     * @param data A pointer to the data to set in the push constant
     * @param size The size of the data in bytes
     * @param offset The offset in bytes into the push constant
     */
    void
    set_push_constants(VkCommandBuffer cmd, VkShaderStageFlagBits shader_stage, const void* data, u32 size, u32 offset);

    /**
     * Set descriptors for this render pass. Should be called within the function that's passed to run.
     * @param cmd The command buffer to write to
     * @param writes The descriptor writes to perform
     */
    virtual void set_descriptors(VkCommandBuffer cmd, const gfx::DescriptorWrites& writes) = 0;

  protected:
    Core&            core_;
    GraphicsBackend& gfx_;
    VkPipelineLayout pipeline_layout_;

    /**
     * Holds info relating to a descriptor
     */
    struct DescriptorInfo {
        u32              set;
        u32              binding;
        VkDescriptorType type;
    };

    /**
     * Holds info related to a push constant
     */
    struct PushConstantsInfo {
        u32                   offset;
        u32                   size;
        VkShaderStageFlagBits stage;

        bool operator==(const PushConstantsInfo& rhs) const {
            return std::tie(offset, size, stage) == std::tie(rhs.offset, rhs.size, rhs.stage);
        }
        bool operator!=(const PushConstantsInfo& rhs) const {
            return !(rhs == *this);
        }
    };

    const std::unordered_map<std::string, DescriptorInfo>& get_descriptors() const {
        return descriptors_;
    }

    const std::vector<PushConstantsInfo>& get_push_constants() const {
        return push_constants_;
    }

    VkDescriptorSetLayout get_descriptor_set_layout(u32 set) {
        auto it = descriptor_set_layouts_.find(set);
        if (it == descriptor_set_layouts_.end()) {
            return VK_NULL_HANDLE;
        }

        return it->second;
    }

  private:
    /**
     * Use shader reflection to get descriptors, push constants, and to create a pipeline layout for this render pass
     * @param shaders The shaders to process that make up this render pass
     */
    void process_shaders(const std::vector<ShaderInfo>& shaders);

    std::unordered_map<std::string, DescriptorInfo> descriptors_;
    std::vector<PushConstantsInfo>                  push_constants_;
    std::unordered_map<u32, VkDescriptorSetLayout>  descriptor_set_layouts_;
};

} // namespace rune::gfx

#endif // RUNE_RENDER_PASS_H
