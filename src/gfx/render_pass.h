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

struct ShaderInfo {
    VkShaderStageFlagBits stage;
    const char*           path;
};

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

class RenderPass {
  public:
    explicit RenderPass(Core& core, GraphicsBackend& gfx, const std::vector<ShaderInfo>& shaders);
    virtual ~RenderPass();

    RenderPass(const RenderPass&) = delete;
    RenderPass& operator=(const RenderPass&) = delete;

    virtual void run(VkCommandBuffer cmd, const std::function<void(VkCommandBuffer)>& func) = 0;

    template <typename T>
    void set_push_constants(VkCommandBuffer cmd, VkShaderStageFlagBits shader_stage, const T& data, u32 offset = 0) {
        set_push_constants(cmd, shader_stage, &data, sizeof(data), offset);
    }

    void
    set_push_constants(VkCommandBuffer cmd, VkShaderStageFlagBits shader_stage, const void* data, u32 size, u32 offset);

    virtual void set_descriptors(VkCommandBuffer cmd, const gfx::DescriptorWrites& writes) = 0;

  protected:
    Core&            core_;
    GraphicsBackend& gfx_;
    VkPipelineLayout pipeline_layout_;

    struct DescriptorInfo {
        u32              set;
        u32              binding;
        VkDescriptorType type;
    };

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
    void process_shaders(const std::vector<ShaderInfo>& shaders);

    std::unordered_map<std::string, DescriptorInfo> descriptors_;
    std::vector<PushConstantsInfo>                  push_constants_;
    std::unordered_map<u32, VkDescriptorSetLayout>  descriptor_set_layouts_;
};

} // namespace rune::gfx

#endif // RUNE_RENDER_PASS_H
