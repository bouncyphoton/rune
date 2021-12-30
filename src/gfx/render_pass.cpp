#include "render_pass.h"

#include "core.h"
#include "utils.h"

#include <spirv_reflect.h>

namespace rune::gfx {

RenderPass::RenderPass(Core& core, GraphicsBackend& gfx, const std::vector<ShaderInfo>& shaders)
    : core_(core), gfx_(gfx) {
    process_shaders(shaders);
}

RenderPass::~RenderPass() {}

void RenderPass::process_shaders(const std::vector<ShaderInfo>& shaders) {
    Logger& logger = core_.get_logger();

    std::vector<SpvReflectShaderModule> modules;
    modules.reserve(shaders.size());
    for (const ShaderInfo& shader : shaders) {
        std::vector<char> shader_data = utils::load_binary_file(shader.path);
        if (shader_data.empty()) {
            logger.fatal("Failed to load shader: '%'", shader.path);
        }

        modules.emplace_back();
        rune_assert(core_,
                    spvReflectCreateShaderModule(shader_data.size(), shader_data.data(), &modules.back()) ==
                        SPV_REFLECT_RESULT_SUCCESS);
    }

    u32 total_sets      = 0;
    u32 total_constants = 0;
    for (SpvReflectShaderModule& module : modules) {
        u32 num_sets;
        spvReflectEnumerateDescriptorSets(&module, &num_sets, nullptr);
        total_sets += num_sets;

        u32 num_constants;
        spvReflectEnumeratePushConstantBlocks(&module, &num_constants, nullptr);
        total_constants += num_constants;
    }
    VkDescriptorSetLayout layouts[total_sets];
    VkPushConstantRange   constant_ranges[total_constants];

    u32 set_start_idx       = 0;
    u32 constants_start_idx = 0;
    for (u32 s = 0; s < shaders.size(); ++s) {
        logger.verbose("info for shader: '%'", shaders[s].path);

        // descriptor sets
        u32 num_sets;
        spvReflectEnumerateDescriptorSets(&modules[s], &num_sets, nullptr);
        SpvReflectDescriptorSet* sets[num_sets];
        spvReflectEnumerateDescriptorSets(&modules[s], &num_sets, sets);
        logger.verbose("- % descriptor set%:", num_sets, num_sets == 1 ? "" : "s");

        for (u32 i = 0; i < num_sets; ++i) {
            SpvReflectDescriptorSet* set = sets[i];

            logger.verbose(" - set %:", set->set);

            VkDescriptorSetLayoutBinding bindings[set->binding_count];
            for (u32 j = 0; j < set->binding_count; ++j) {
                SpvReflectDescriptorBinding* binding = set->bindings[j];
                logger.verbose("  - binding %: '%'", binding->binding, binding->name);

                bindings[j].binding            = binding->binding;
                bindings[j].descriptorType     = static_cast<VkDescriptorType>(binding->descriptor_type);
                bindings[j].descriptorCount    = binding->count;
                bindings[j].stageFlags         = shaders[s].stage;
                bindings[j].pImmutableSamplers = nullptr;

                DescriptorInfo descriptor_info = {};
                descriptor_info.set            = binding->set;
                descriptor_info.binding        = binding->binding;
                descriptor_info.type           = static_cast<VkDescriptorType>(binding->descriptor_type);
                descriptors_[binding->name]    = descriptor_info;
            }

            VkDescriptorSetLayoutCreateInfo set_info = {};
            set_info.sType                           = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            set_info.bindingCount                    = set->binding_count;
            set_info.pBindings                       = bindings;

            layouts[set_start_idx + i] = gfx_.create_descriptor_set_layout(set_info);
        }
        set_start_idx += num_sets;

        // push constants
        u32 num_constants;
        spvReflectEnumeratePushConstantBlocks(&modules[s], &num_constants, nullptr);
        SpvReflectBlockVariable* push_variables[num_constants];
        spvReflectEnumeratePushConstantBlocks(&modules[s], &num_constants, push_variables);
        logger.verbose("- % push constant%:", num_constants, num_constants == 1 ? "" : "s");

        for (u32 i = 0; i < num_constants; ++i) {
            SpvReflectBlockVariable* push_variable = push_variables[i];
            logger.verbose(" - '%', offset: %, size: %",
                           push_variable->name,
                           push_variable->offset,
                           push_variable->size);
            constant_ranges[constants_start_idx + i].offset     = push_variable->offset;
            constant_ranges[constants_start_idx + i].size       = push_variable->size;
            constant_ranges[constants_start_idx + i].stageFlags = shaders[s].stage;

            PushConstantsInfo push_constant_info = {};
            push_constant_info.offset            = push_variable->offset;
            push_constant_info.size              = push_variable->size;
            push_constant_info.stage             = shaders[s].stage;
            push_constants_.emplace_back(push_constant_info);
        }
        constants_start_idx += num_constants;
    }

    // create VkPipelineLayout
    VkPipelineLayoutCreateInfo pipeline_layout_info = {};
    pipeline_layout_info.sType                      = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount             = count_of(layouts);
    pipeline_layout_info.pSetLayouts                = layouts;
    pipeline_layout_info.pushConstantRangeCount     = count_of(constant_ranges);
    pipeline_layout_info.pPushConstantRanges        = constant_ranges;

    pipeline_layout_ = gfx_.create_pipeline_layout(pipeline_layout_info);

    for (SpvReflectShaderModule& module : modules) {
        spvReflectDestroyShaderModule(&module);
    }
}

void RenderPass::set_push_constants(VkCommandBuffer       cmd,
                                    VkShaderStageFlagBits shader_stage,
                                    const void*           data,
                                    u32                   size,
                                    u32                   offset) {
    if constexpr (!consts::is_release) {
        // validate that we're updating in a push constant range
        bool valid = false;
        for (const PushConstantsInfo& info : push_constants_) {
            if (info.stage != shader_stage) {
                continue;
            }

            if (offset + size <= info.size + info.offset) {
                valid = true;
                break;
            }
        }

        if (!valid) {
            core_.get_logger().warn("Invalid push constant with size: %, offset: %, shader stage: %",
                                    size,
                                    offset,
                                    shader_stage);
            return;
        }
    }
    vkCmdPushConstants(cmd, pipeline_layout_, shader_stage, offset, size, data);
}

} // namespace rune::gfx
