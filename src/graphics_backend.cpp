#include "graphics_backend.h"

#include "core.h"
#include "utils.h"

#include <fstream>
#include <GLFW/glfw3.h>
#include <set>
#include <spirv_reflect.h>

#define vk_check(expr) rune_assert(core_, (expr) == VK_SUCCESS)

namespace rune {

constexpr const char*              g_instance_layers[]            = {"VK_LAYER_KHRONOS_validation"};
constexpr const char*              g_required_device_extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
constexpr VkPhysicalDeviceFeatures g_required_device_features     = {};

GraphicsBackend::GraphicsBackend(Core& core, GLFWwindow* window) : core_(core) {
    // create instance
    VkApplicationInfo app_info = {};
    app_info.sType             = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pEngineName       = "rune";
    app_info.apiVersion        = VK_API_VERSION_1_2;

    VkInstanceCreateInfo instance_info    = {};
    instance_info.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pApplicationInfo        = &app_info;
    instance_info.enabledLayerCount       = count_of(g_instance_layers);
    instance_info.ppEnabledLayerNames     = g_instance_layers;
    instance_info.ppEnabledExtensionNames = glfwGetRequiredInstanceExtensions(&instance_info.enabledExtensionCount);

    vk_check(vkCreateInstance(&instance_info, nullptr, &instance_));
    cleanup_.emplace([=]() { vkDestroyInstance(instance_, nullptr); });

    // create surface
    vk_check(glfwCreateWindowSurface(instance_, window, nullptr, &surface_));
    cleanup_.emplace([=]() { vkDestroySurfaceKHR(instance_, surface_, nullptr); });

    choose_physical_device();
    create_logical_device();
    create_swapchain();

    // create command pool, single threaded rendering for now
    VkCommandPoolCreateInfo command_pool_create_info = {};
    command_pool_create_info.sType                   = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_pool_create_info.queueFamilyIndex        = graphics_family_index_;
    command_pool_create_info.flags                   = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    vk_check(vkCreateCommandPool(device_, &command_pool_create_info, nullptr, &command_pool_));
    cleanup_.emplace([=]() { vkDestroyCommandPool(device_, command_pool_, nullptr); });

    // create per-frame data
    for (u32 i = 0; i < NUM_FRAMES_IN_FLIGHT; ++i) {
        VkCommandBufferAllocateInfo cmd_buf_alloc_info = {};
        cmd_buf_alloc_info.sType                       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmd_buf_alloc_info.commandPool                 = command_pool_;
        cmd_buf_alloc_info.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmd_buf_alloc_info.commandBufferCount          = 1;

        VkCommandBuffer cmd_buf;
        vk_check(vkAllocateCommandBuffers(device_, &cmd_buf_alloc_info, &cmd_buf));
        frames_[i].command_buffer_ = cmd_buf;
        cleanup_.emplace([=]() { vkFreeCommandBuffers(device_, command_pool_, 1, &cmd_buf); });

        VkSemaphoreCreateInfo semaphore_create_info = {};
        semaphore_create_info.sType                 = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fence_create_info = {};
        fence_create_info.sType             = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_create_info.flags             = VK_FENCE_CREATE_SIGNALED_BIT;

        VkSemaphore img_available, render_finished;
        VkFence     in_flight;

        vk_check(vkCreateSemaphore(device_, &semaphore_create_info, nullptr, &img_available));
        vk_check(vkCreateSemaphore(device_, &semaphore_create_info, nullptr, &render_finished));
        vk_check(vkCreateFence(device_, &fence_create_info, nullptr, &in_flight));
        frames_[i].image_available_ = img_available;
        frames_[i].render_finished_ = render_finished;
        frames_[i].in_flight_       = in_flight;
        cleanup_.emplace([=]() {
            vkDestroyFence(device_, in_flight, nullptr);
            vkDestroySemaphore(device_, render_finished, nullptr);
            vkDestroySemaphore(device_, img_available, nullptr);
        });
    }

    // todo: swapchain resizing
    // todo: passes
}

GraphicsBackend::~GraphicsBackend() {
    vkDeviceWaitIdle(device_);

    for (auto [render_pass, framebuffers] : framebuffers_) {
        vkDestroyRenderPass(device_, render_pass, nullptr);
        for (VkFramebuffer framebuffer : framebuffers) {
            vkDestroyFramebuffer(device_, framebuffer, nullptr);
        }
    }

    while (!cleanup_.empty()) {
        cleanup_.top()();
        cleanup_.pop();
    }
}

void GraphicsBackend::begin_frame() {
    // wait for previous frame to finish
    vkWaitForFences(device_, 1, &get_current_frame().in_flight_, VK_TRUE, UINT64_MAX);
    vkResetFences(device_, 1, &get_current_frame().in_flight_);

    // commands finished executing, can do things safely
    vk_check(vkResetCommandBuffer(get_current_frame().command_buffer_, 0));

    // get next image
    vk_check(vkAcquireNextImageKHR(device_,
                                   swapchain_,
                                   UINT64_MAX,
                                   get_current_frame().image_available_,
                                   VK_NULL_HANDLE,
                                   &swap_image_index_));

    // start next frame
    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vk_check(vkBeginCommandBuffer(get_current_frame().command_buffer_, &begin_info));
}

void GraphicsBackend::end_frame() {
    vk_check(vkEndCommandBuffer(get_current_frame().command_buffer_));

    // wait for image to be available, submit queue, signal render_finished_ when done
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSubmitInfo         submit_info   = {};
    submit_info.sType                  = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount     = 1;
    submit_info.pWaitSemaphores        = &get_current_frame().image_available_;
    submit_info.pWaitDstStageMask      = wait_stages;
    submit_info.commandBufferCount     = 1;
    submit_info.pCommandBuffers        = &get_current_frame().command_buffer_;
    submit_info.signalSemaphoreCount   = 1;
    submit_info.pSignalSemaphores      = &get_current_frame().render_finished_;
    vk_check(vkQueueSubmit(graphics_queue_, 1, &submit_info, get_current_frame().in_flight_));

    // wait for render_finished_ and queue for presentation
    VkPresentInfoKHR present_info   = {};
    present_info.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores    = &get_current_frame().render_finished_;
    present_info.swapchainCount     = 1;
    present_info.pSwapchains        = &swapchain_;
    present_info.pImageIndices      = &swap_image_index_;
    vk_check(vkQueuePresentKHR(present_queue_, &present_info));

    current_frame_ = (current_frame_ + 1) % NUM_FRAMES_IN_FLIGHT;
}

GraphicsPass GraphicsBackend::create_pass(GraphicsPassDesc desc) {
    Logger& logger = core_.get_logger();

    GraphicsPass pass = {};
    if (desc.render_area.extent.width == 0 || desc.render_area.extent.height == 0) {
        pass.render_area = {0, 0, core_.get_config().get_window_width(), core_.get_config().get_window_height()};
    } else {
        pass.render_area = desc.render_area;
    }

    struct ShaderReflectionWrapper {
        ShaderReflectionWrapper(Core& core, const std::vector<char>& code) {
            rune_assert(core,
                        spvReflectCreateShaderModule(code.size(), code.data(), &module) == SPV_REFLECT_RESULT_SUCCESS);
        }
        ~ShaderReflectionWrapper() {
            spvReflectDestroyShaderModule(&module);
        }

        SpvReflectShaderModule module;
    };

    // load shader modules
    ShaderData vertex_shader_data     = create_shader_module(desc.vert_shader_path);
    ShaderData fragment_shader_module = create_shader_module(desc.frag_shader_path);

    // load reflection data
    ShaderReflectionWrapper vert_refl(core_, vertex_shader_data.code);
    ShaderReflectionWrapper frag_refl(core_, fragment_shader_module.code);

    // get descriptor sets data
    u32 num_vert_sets, num_frag_sets;
    spvReflectEnumerateDescriptorSets(&vert_refl.module, &num_vert_sets, nullptr);
    spvReflectEnumerateDescriptorSets(&frag_refl.module, &num_frag_sets, nullptr);
    SpvReflectDescriptorSet* sets[num_vert_sets + num_frag_sets];
    VkDescriptorSetLayout    layouts[num_vert_sets + num_frag_sets];
    spvReflectEnumerateDescriptorSets(&vert_refl.module, &num_vert_sets, sets);
    spvReflectEnumerateDescriptorSets(&frag_refl.module, &num_frag_sets, sets + num_vert_sets);

    // create descriptor set layouts from reflection data
    for (u32 i = 0; i < num_vert_sets + num_frag_sets; ++i) {
        SpvReflectDescriptorSet* set = sets[i];

        if (i == 0) {
            logger.verbose("shader descriptor sets: %", desc.vert_shader_path);
        } else if (i == num_vert_sets) {
            logger.verbose("shader descriptor sets: %", desc.frag_shader_path);
        }

        logger.verbose("- set %:", set->set);

        VkDescriptorSetLayoutBinding bindings[set->binding_count];
        for (u32 j = 0; j < set->binding_count; ++j) {
            SpvReflectDescriptorBinding* binding = set->bindings[j];
            logger.verbose(" - binding %: %", binding->binding, binding->name);

            bindings[j].binding         = binding->binding;
            bindings[j].descriptorType  = static_cast<VkDescriptorType>(binding->descriptor_type);
            bindings[j].descriptorCount = binding->count;
            bindings[j].stageFlags      = i < num_vert_sets ? VK_SHADER_STAGE_VERTEX_BIT : VK_SHADER_STAGE_FRAGMENT_BIT;
            bindings[j].pImmutableSamplers = nullptr;
        }

        VkDescriptorSetLayoutCreateInfo set_info = {};
        set_info.sType                           = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        set_info.bindingCount                    = set->binding_count;
        set_info.pBindings                       = bindings;

        VkDescriptorSetLayout layout;
        vkCreateDescriptorSetLayout(device_, &set_info, nullptr, &layout);
        layouts[i] = layout;
        cleanup_.emplace([=] { vkDestroyDescriptorSetLayout(device_, layout, nullptr); });
    }

    // create push constant ranges
    u32 num_vert_push, num_frag_push;
    spvReflectEnumeratePushConstantBlocks(&vert_refl.module, &num_vert_push, nullptr);
    spvReflectEnumeratePushConstantBlocks(&frag_refl.module, &num_frag_push, nullptr);
    SpvReflectBlockVariable* push_variables[num_vert_push + num_frag_push];
    VkPushConstantRange      constant_ranges[num_vert_push + num_frag_push];
    spvReflectEnumeratePushConstantBlocks(&vert_refl.module, &num_vert_push, push_variables);
    spvReflectEnumeratePushConstantBlocks(&frag_refl.module, &num_frag_push, push_variables + num_vert_push);
    for (u32 i = 0; i < num_vert_push + num_frag_push; ++i) {
        if (i == 0) {
            logger.verbose("shader constants: %", desc.vert_shader_path);
        } else if (i == num_vert_sets) {
            logger.verbose("shader constants: %", desc.frag_shader_path);
        }
        logger.verbose("- %, offset: %, size: %",
                       push_variables[i]->name,
                       push_variables[i]->offset,
                       push_variables[i]->size);
        constant_ranges[i].offset     = push_variables[i]->offset;
        constant_ranges[i].size       = push_variables[i]->size;
        constant_ranges[i].stageFlags = i < num_vert_push ? VK_SHADER_STAGE_VERTEX_BIT : VK_SHADER_STAGE_FRAGMENT_BIT;
    }

    // create VkPipelineLayout
    VkPipelineLayoutCreateInfo pipeline_layout_info = {};
    pipeline_layout_info.sType                      = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount             = count_of(layouts);
    pipeline_layout_info.pSetLayouts                = layouts;
    pipeline_layout_info.pushConstantRangeCount     = count_of(constant_ranges);
    pipeline_layout_info.pPushConstantRanges        = constant_ranges;
    vk_check(vkCreatePipelineLayout(device_, &pipeline_layout_info, nullptr, &pass.pipeline_layout));
    cleanup_.emplace([=] { vkDestroyPipelineLayout(device_, pass.pipeline_layout, nullptr); });

    // TODO: generate graphics pipeline

    // TODO: allocate VkDescriptorSet using VkDescriptorSetLayout (only need 1 per frame because bindless)

    // generate framebuffers
    VkAttachmentDescription color_attachment = {};
    color_attachment.format                  = swapchain_format_.format;
    color_attachment.samples                 = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp                  = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp                 = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp           = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp          = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout             = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment_ref = {};
    color_attachment_ref.attachment            = 0;
    color_attachment_ref.layout                = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments    = &color_attachment_ref;

    VkRenderPassCreateInfo render_pass_create_info = {};
    render_pass_create_info.sType                  = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_create_info.pAttachments           = &color_attachment;
    render_pass_create_info.attachmentCount        = 1;
    render_pass_create_info.pSubpasses             = &subpass;
    render_pass_create_info.subpassCount           = 1;
    vk_check(vkCreateRenderPass(device_, &render_pass_create_info, nullptr, &pass.render_pass));

    VkFramebufferCreateInfo framebuffer_create_info = {};
    framebuffer_create_info.sType                   = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_create_info.renderPass              = pass.render_pass;
    framebuffer_create_info.attachmentCount         = 1;
    framebuffer_create_info.width                   = pass.render_area.extent.width;
    framebuffer_create_info.height                  = pass.render_area.extent.height;
    framebuffer_create_info.layers                  = 1;

    std::vector<VkFramebuffer>& framebuffers = framebuffers_[pass.render_pass];
    framebuffers                             = std::vector<VkFramebuffer>(swapchain_image_views_.size());

    for (u32 i = 0; i < swapchain_image_views_.size(); ++i) {
        framebuffer_create_info.pAttachments = &swapchain_image_views_[i];
        vk_check(vkCreateFramebuffer(device_, &framebuffer_create_info, nullptr, &framebuffers[i]));
    }

    return pass;
}

void GraphicsBackend::start_pass(const GraphicsPass& pass) {
    VkClearValue clear_value = {};
    clear_value.color        = {0, 0, 0, 1};

    VkRenderPassBeginInfo begin_info = {};
    begin_info.sType                 = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin_info.renderPass            = pass.render_pass;
    begin_info.framebuffer           = framebuffers_.at(pass.render_pass)[swap_image_index_];
    begin_info.renderArea            = pass.render_area;
    begin_info.clearValueCount       = 1;
    begin_info.pClearValues          = &clear_value;

    vkCmdBeginRenderPass(get_command_buffer(), &begin_info, VK_SUBPASS_CONTENTS_INLINE);
}

void GraphicsBackend::end_pass() {
    vkCmdEndRenderPass(get_command_buffer());
}

void GraphicsBackend::choose_physical_device() {
    // get physical devices
    u32 num_physical_devices;
    vk_check(vkEnumeratePhysicalDevices(instance_, &num_physical_devices, nullptr));
    rune_assert(core_, num_physical_devices > 0);
    VkPhysicalDevice physical_devices[num_physical_devices];
    vk_check(vkEnumeratePhysicalDevices(instance_, &num_physical_devices, physical_devices));

    core_.get_logger().info("% physical device%", num_physical_devices, num_physical_devices == 1 ? "" : "s");

    // find device with requirements
    for (VkPhysicalDevice possible_device : physical_devices) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(possible_device, &properties);

        core_.get_logger().info("- %", properties.deviceName);

        u32 num_device_extensions;
        vkEnumerateDeviceExtensionProperties(possible_device, nullptr, &num_device_extensions, nullptr);
        VkExtensionProperties device_extensions[num_device_extensions];
        vkEnumerateDeviceExtensionProperties(possible_device, nullptr, &num_device_extensions, device_extensions);

        // make sure that all required extensions are present
        bool usable = true;
        for (const char* required_extension : g_required_device_extensions) {
            bool found = false;
            for (VkExtensionProperties extension : device_extensions) {
                std::string name = std::string(extension.extensionName);
                if (name == required_extension) {
                    found = true;
                    break;
                }
            }

            if (!found) {
                usable = false;
                break;
            }
        }

        if (!usable) {
            continue;
        }

        u32 num_present_modes;
        vkGetPhysicalDeviceSurfacePresentModesKHR(possible_device, surface_, &num_present_modes, nullptr);
        if (num_present_modes == 0) {
            continue;
        }

        VkSurfaceCapabilitiesKHR capabilities;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(possible_device, surface_, &capabilities);
        if ((capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) == 0) {
            continue;
        }

        u32 num_queue_families;
        vkGetPhysicalDeviceQueueFamilyProperties(possible_device, &num_queue_families, nullptr);
        VkQueueFamilyProperties queue_families[num_queue_families];
        vkGetPhysicalDeviceQueueFamilyProperties(possible_device, &num_queue_families, queue_families);

        // possible queue family indices
        std::optional<u32> possible_graphics;
        std::optional<u32> possible_compute;
        std::optional<u32> possible_present;

        for (u32 i = 0; i < num_queue_families; ++i) {
            if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                possible_graphics = i;
            }

            if (queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
                possible_compute = i;
            }

            VkBool32 present;
            vkGetPhysicalDeviceSurfaceSupportKHR(possible_device, i, surface_, &present);
            if (present) {
                possible_present = i;
            }
        }

        core_.get_logger().info(" - graphics queue family present: %", possible_graphics.has_value());
        core_.get_logger().info(" - compute queue family present: %", possible_compute.has_value());
        core_.get_logger().info(" - present queue family present: %", possible_present.has_value());

        if (!possible_graphics || !possible_compute || !possible_present) {
            continue;
        }

        physical_device_       = possible_device;
        graphics_family_index_ = *possible_graphics;
        compute_family_index_  = *possible_compute;
        present_family_index_  = *possible_present;
        break;
    }

    if (physical_device_ == VK_NULL_HANDLE) {
        core_.get_logger().fatal("unable to find suitable physical device");
    }
}

void GraphicsBackend::create_logical_device() {
    std::set<unsigned> unique_indices = {graphics_family_index_, compute_family_index_, present_family_index_};

    float                   queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_infos[unique_indices.size()];
    unsigned                num_queue_infos = 0;
    for (unsigned index : unique_indices) {
        VkDeviceQueueCreateInfo queue_info = {};
        queue_info.sType                   = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info.queueFamilyIndex        = index;
        queue_info.queueCount              = 1;
        queue_info.pQueuePriorities        = &queue_priority;

        queue_infos[num_queue_infos++] = queue_info;
    }

    VkDeviceCreateInfo device_info      = {};
    device_info.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.pQueueCreateInfos       = queue_infos;
    device_info.queueCreateInfoCount    = num_queue_infos;
    device_info.pEnabledFeatures        = &g_required_device_features;
    device_info.enabledExtensionCount   = count_of(g_required_device_extensions);
    device_info.ppEnabledExtensionNames = g_required_device_extensions;

    vk_check(vkCreateDevice(physical_device_, &device_info, nullptr, &device_));
    cleanup_.emplace([=]() { vkDestroyDevice(device_, nullptr); });

    vkGetDeviceQueue(device_, graphics_family_index_, 0, &graphics_queue_);
    vkGetDeviceQueue(device_, compute_family_index_, 0, &compute_queue_);
    vkGetDeviceQueue(device_, present_family_index_, 0, &present_queue_);
}

void GraphicsBackend::create_swapchain() {
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device_, surface_, &capabilities);

    // clamp image count to surface capability bounds
    u32 image_count = std::max(NUM_FRAMES_IN_FLIGHT, capabilities.minImageCount);
    if (capabilities.maxImageCount != 0) {
        image_count = std::min(image_count, capabilities.maxImageCount);
    }

    // clamp extent to capability bounds
    swapchain_extent_ = capabilities.currentExtent;
    if (swapchain_extent_.width == UINT32_MAX && swapchain_extent_.height == UINT32_MAX) {
        swapchain_extent_.width  = std::clamp(core_.get_config().get_window_width(),
                                             capabilities.minImageExtent.width,
                                             capabilities.maxImageExtent.width);
        swapchain_extent_.height = std::clamp(core_.get_config().get_window_height(),
                                              capabilities.minImageExtent.height,
                                              capabilities.maxImageExtent.height);
    }

    VkCompositeAlphaFlagBitsKHR composite_alpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    // prefer VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR
    if (capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) {
        composite_alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    } else if (capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR) {
        composite_alpha = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
    } else if (capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR) {
        composite_alpha = VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;
    }

    // get format
    u32 num_formats;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, surface_, &num_formats, nullptr);
    VkSurfaceFormatKHR formats[num_formats];
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, surface_, &num_formats, formats);
    rune_assert(core_, num_formats > 0);

    VkSurfaceFormatKHR surface_format = formats[0];
    for (VkSurfaceFormatKHR f : formats) {
        // prefer RGBA8_UNORM and SRGB color space
        if (f.format == VK_FORMAT_R8G8B8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surface_format = f;
            break;
        }
    }
    swapchain_format_ = surface_format;

    // get present mode
    u32 num_present_modes;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device_, surface_, &num_present_modes, nullptr);
    VkPresentModeKHR present_modes[num_present_modes];
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device_, surface_, &num_present_modes, present_modes);

    // FIFO always supported
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
    for (VkPresentModeKHR m : present_modes) {
        // would prefer mailbox
        if (m == VK_PRESENT_MODE_MAILBOX_KHR) {
            present_mode = m;
            break;
        }
    }

    VkSharingMode image_sharing;
    u32           num_queue_families;
    u32*          p_queue_families;
    u32           queue_families[] = {graphics_family_index_, present_family_index_};
    if (graphics_family_index_ == present_family_index_) {
        image_sharing      = VK_SHARING_MODE_EXCLUSIVE;
        num_queue_families = 0;
        p_queue_families   = nullptr;
    } else {
        image_sharing      = VK_SHARING_MODE_CONCURRENT;
        num_queue_families = count_of(queue_families);
        p_queue_families   = queue_families;
    }

    VkSwapchainCreateInfoKHR swapchain_create_info = {};
    swapchain_create_info.sType                    = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_create_info.surface                  = surface_;
    swapchain_create_info.minImageCount            = image_count;
    swapchain_create_info.imageFormat              = surface_format.format;
    swapchain_create_info.imageColorSpace          = surface_format.colorSpace;
    swapchain_create_info.imageExtent              = swapchain_extent_;
    swapchain_create_info.imageArrayLayers         = 1;
    swapchain_create_info.imageUsage            = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    swapchain_create_info.imageSharingMode      = image_sharing;
    swapchain_create_info.queueFamilyIndexCount = num_queue_families;
    swapchain_create_info.pQueueFamilyIndices   = p_queue_families;
    swapchain_create_info.preTransform          = capabilities.currentTransform;
    swapchain_create_info.compositeAlpha        = composite_alpha;
    swapchain_create_info.presentMode           = present_mode;
    swapchain_create_info.clipped               = VK_TRUE; // Allow for swapchain to not own all of its pixels

    vk_check(vkCreateSwapchainKHR(device_, &swapchain_create_info, nullptr, &swapchain_));
    cleanup_.emplace([=]() { vkDestroySwapchainKHR(device_, swapchain_, nullptr); });

    u32 num_swapchain_images;
    vk_check(vkGetSwapchainImagesKHR(device_, swapchain_, &num_swapchain_images, nullptr));
    swapchain_images_.resize(num_swapchain_images);
    swapchain_image_views_.resize(num_swapchain_images);
    vk_check(vkGetSwapchainImagesKHR(device_, swapchain_, &num_swapchain_images, swapchain_images_.data()));
    for (u32 i = 0; i < num_swapchain_images; ++i) {
        VkImageViewCreateInfo image_view_create_info = {};
        image_view_create_info.sType                 = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        image_view_create_info.image                 = swapchain_images_[i];
        image_view_create_info.viewType              = VK_IMAGE_VIEW_TYPE_2D;
        image_view_create_info.format                = swapchain_format_.format;

        // our swapchain images are color targets without mipmapping or multiple layers
        image_view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_view_create_info.subresourceRange.levelCount = 1;
        image_view_create_info.subresourceRange.layerCount = 1;

        VkImageView view;
        vk_check(vkCreateImageView(device_, &image_view_create_info, nullptr, &view));
        swapchain_image_views_[i] = view;
        cleanup_.emplace([=]() { vkDestroyImageView(device_, view, nullptr); });
    }
}

GraphicsBackend::ShaderData GraphicsBackend::create_shader_module(const char* shader_path) {
    std::vector<char> code;

    if (std::ifstream file = std::ifstream(shader_path, std::ios::ate | std::ios::binary)) {
        u32 size = (u32)file.tellg();
        code.resize(size);
        file.seekg(0);
        file.read(code.data(), size);
    } else {
        core_.get_logger().fatal("Failed to load shader: '%'", shader_path);
    }

    VkShaderModuleCreateInfo create_info = {};
    create_info.sType                    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.flags                    = 0;
    create_info.codeSize                 = code.size();
    create_info.pCode                    = reinterpret_cast<u32*>(code.data());

    VkShaderModule shader_module;
    vk_check(vkCreateShaderModule(device_, &create_info, nullptr, &shader_module));
    cleanup_.emplace([=] { vkDestroyShaderModule(device_, shader_module, nullptr); });

    return {shader_module, code};
}

} // namespace rune
