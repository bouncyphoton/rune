#include "graphics_backend.h"

#include "core.h"
#include "utils.h"

#include <fstream>
#include <GLFW/glfw3.h>
#include <set>
#include <spirv_reflect.h>

#define vk_check(expr) rune_assert(core_, (expr) == VK_SUCCESS)

namespace rune::gfx {

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
    for (PerFrame& frame : frames_) {
        VkCommandBufferAllocateInfo cmd_buf_alloc_info = {};
        cmd_buf_alloc_info.sType                       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmd_buf_alloc_info.commandPool                 = command_pool_;
        cmd_buf_alloc_info.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmd_buf_alloc_info.commandBufferCount          = 1;

        VkCommandBuffer cmd_buf;
        vk_check(vkAllocateCommandBuffers(device_, &cmd_buf_alloc_info, &cmd_buf));
        frame.command_buffer_ = cmd_buf;
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
        frame.image_available_ = img_available;
        frame.render_finished_ = render_finished;
        frame.in_flight_       = in_flight;
        cleanup_.emplace([=]() {
            vkDestroyFence(device_, in_flight, nullptr);
            vkDestroySemaphore(device_, render_finished, nullptr);
            vkDestroySemaphore(device_, img_available, nullptr);
        });
    }

    // todo: swapchain resizing
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

        core_.get_logger().info(" - graphics queue family present: %",
                                possible_graphics.has_value() ? "true" : "false");
        core_.get_logger().info(" - compute queue family present: %", possible_compute.has_value() ? "true" : "false");
        core_.get_logger().info(" - present queue family present: %", possible_present.has_value() ? "true" : "false");

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

VkRenderPass GraphicsBackend::create_render_pass() {
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

    VkRenderPass render_pass;
    vk_check(vkCreateRenderPass(device_, &render_pass_create_info, nullptr, &render_pass));
    return render_pass;
}

void GraphicsBackend::create_framebuffers(VkRenderPass render_pass, VkRect2D render_area) {
    VkFramebufferCreateInfo framebuffer_create_info = {};
    framebuffer_create_info.sType                   = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_create_info.renderPass              = render_pass;
    framebuffer_create_info.attachmentCount         = 1;
    framebuffer_create_info.width                   = render_area.extent.width;
    framebuffer_create_info.height                  = render_area.extent.height;
    framebuffer_create_info.layers                  = 1;

    std::vector<VkFramebuffer>& framebuffers = framebuffers_[render_pass];

    framebuffers = std::vector<VkFramebuffer>(swapchain_image_views_.size());

    for (u32 i = 0; i < swapchain_image_views_.size(); ++i) {
        framebuffer_create_info.pAttachments = &swapchain_image_views_[i];
        vk_check(vkCreateFramebuffer(device_, &framebuffer_create_info, nullptr, &framebuffers[i]));
    }
}

VkFramebuffer GraphicsBackend::get_framebuffer(VkRenderPass render_pass) {
    return framebuffers_.at(render_pass).at(swap_image_index_);
}

VkDescriptorSetLayout GraphicsBackend::create_descriptor_set_layout(const VkDescriptorSetLayoutCreateInfo& set_info) {
    VkDescriptorSetLayout layout;
    vkCreateDescriptorSetLayout(device_, &set_info, nullptr, &layout);
    cleanup_.emplace([=] { vkDestroyDescriptorSetLayout(device_, layout, nullptr); });
    return layout;
}

VkPipelineLayout GraphicsBackend::create_pipeline_layout(const VkPipelineLayoutCreateInfo& pipeline_layout_info) {
    VkPipelineLayout pipeline_layout;
    vk_check(vkCreatePipelineLayout(device_, &pipeline_layout_info, nullptr, &pipeline_layout));
    cleanup_.emplace([=] { vkDestroyPipelineLayout(device_, pipeline_layout, nullptr); });
    return pipeline_layout;
}
VkPipeline GraphicsBackend::create_graphics_pipeline(const std::vector<ShaderInfo>& shaders,
                                                     VkPipelineLayout               pipeline_layout,
                                                     VkRenderPass                   render_pass) {
    std::vector<VkPipelineShaderStageCreateInfo> stages(shaders.size());
    for (u32 i = 0; i < shaders.size(); ++i) {
        stages[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[i].pName = "main";
        stages[i].stage = shaders[i].stage;

        std::vector<char> code = utils::load_binary_file(shaders[i].path);

        VkShaderModuleCreateInfo shader_module_create_info = {};
        shader_module_create_info.sType                    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shader_module_create_info.codeSize                 = code.size();
        shader_module_create_info.pCode                    = reinterpret_cast<const uint32_t*>(code.data());
        vk_check(vkCreateShaderModule(device_, &shader_module_create_info, nullptr, &stages[i].module));
    }

    VkPipelineVertexInputStateCreateInfo vertex_input = {};
    vertex_input.sType                                = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo input_assembly = {};
    input_assembly.sType                                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology                               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // Viewport
    VkViewport viewport = {};
    viewport.x          = 0.0f;
    viewport.y          = 0.0f;
    viewport.width      = (f32)swapchain_extent_.width;
    viewport.height     = (f32)swapchain_extent_.height;
    viewport.minDepth   = 0.0f;
    viewport.maxDepth   = 1.0f;

    VkRect2D scissor = {};
    scissor.offset   = {0, 0};
    scissor.extent   = swapchain_extent_;

    VkPipelineViewportStateCreateInfo viewport_state = {};
    viewport_state.sType                             = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount                     = 1;
    viewport_state.pViewports                        = &viewport;
    viewport_state.scissorCount                      = 1;
    viewport_state.pScissors                         = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType                                  = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.depthClampEnable                       = VK_FALSE;
    rasterization.rasterizerDiscardEnable                = VK_FALSE;
    rasterization.polygonMode                            = VK_POLYGON_MODE_FILL;
    rasterization.lineWidth                              = 1.0f;
    rasterization.cullMode                               = VK_CULL_MODE_BACK_BIT;
    rasterization.frontFace                              = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.depthBiasEnable                        = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType                                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples                 = VK_SAMPLE_COUNT_1_BIT;
    multisample.minSampleShading                     = 1.0f;

    VkPipelineColorBlendAttachmentState blend_attachment = {};
    blend_attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blend_attachment.blendEnable         = VK_TRUE;
    blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    blend_attachment.colorBlendOp        = VK_BLEND_OP_ADD;
    blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    blend_attachment.alphaBlendOp        = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo color_blend_state = {};
    color_blend_state.sType                               = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend_state.attachmentCount                     = 1;
    color_blend_state.pAttachments                        = &blend_attachment;

    VkGraphicsPipelineCreateInfo graphics_pipeline_ci = {};
    graphics_pipeline_ci.sType                        = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    graphics_pipeline_ci.stageCount                   = stages.size();
    graphics_pipeline_ci.pStages                      = stages.data();
    graphics_pipeline_ci.pVertexInputState            = &vertex_input;
    graphics_pipeline_ci.pInputAssemblyState          = &input_assembly;
    graphics_pipeline_ci.pViewportState               = &viewport_state;
    graphics_pipeline_ci.pRasterizationState          = &rasterization;
    graphics_pipeline_ci.pMultisampleState            = &multisample;
    graphics_pipeline_ci.pDepthStencilState           = nullptr;
    graphics_pipeline_ci.pColorBlendState             = &color_blend_state;
    graphics_pipeline_ci.pDynamicState                = nullptr;
    graphics_pipeline_ci.layout                       = pipeline_layout;
    graphics_pipeline_ci.renderPass                   = render_pass;
    graphics_pipeline_ci.subpass                      = 0;
    graphics_pipeline_ci.basePipelineHandle           = VK_NULL_HANDLE;
    graphics_pipeline_ci.basePipelineIndex            = -1;

    VkPipeline pipeline;
    vk_check(vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &graphics_pipeline_ci, nullptr, &pipeline));
    cleanup_.emplace([=] { vkDestroyPipeline(device_, pipeline, nullptr); });

    for (VkPipelineShaderStageCreateInfo& stage : stages) {
        vkDestroyShaderModule(device_, stage.module, nullptr);
    }

    return pipeline;
}

} // namespace rune::gfx