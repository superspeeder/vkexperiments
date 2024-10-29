#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include "render_context.hpp"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <shaderc/shaderc.hpp>

#include <optional>
#include <unordered_set>
#include <fstream>




VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE;

namespace vke {
    RenderContext::RenderContext(GLFWwindow *window) {
        setup_validation_logger();
        VULKAN_HPP_DEFAULT_DISPATCHER.init();

        vk::ApplicationInfo app_info{};
        app_info.setApiVersion(vk::ApiVersion13);

        std::vector<const char *> instance_extensions = {VK_EXT_DEBUG_UTILS_EXTENSION_NAME};

        std::vector<const char *> instance_layers = {"VK_LAYER_KHRONOS_validation"};
        {
            uint32_t     count;
            const char **required = glfwGetRequiredInstanceExtensions(&count);
            instance_extensions.insert(instance_extensions.end(), required, required + count);
        }

        vk::DebugUtilsMessengerCreateInfoEXT messenger_create_info{};
        messenger_create_info.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning;
        messenger_create_info.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
            vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::eDeviceAddressBinding;
        messenger_create_info.pfnUserCallback = validation_callback;

        m_instance = vk::createInstance(vk::InstanceCreateInfo({}, &app_info, instance_layers, instance_extensions, &messenger_create_info));
        VULKAN_HPP_DEFAULT_DISPATCHER.init(m_instance);

        m_debug_messenger = m_instance.createDebugUtilsMessengerEXT(messenger_create_info);
        {
            VkSurfaceKHR s;
            glfwCreateWindowSurface(m_instance, window, nullptr, &s);
            m_surface = s;
        }

        m_physical_device = m_instance.enumeratePhysicalDevices()[0];
        {
            std::optional<uint32_t> graphics;
            std::optional<uint32_t> present;
            std::optional<uint32_t> transfer;
            std::optional<uint32_t> compute;
            bool                    sparseTransfer        = false;
            bool                    sparseGraphics        = false;
            bool                    sparseCompute         = false; // nice to have sparse compute too
            bool                    computeGraphicsShared = false;

            const auto queue_family_properties = m_physical_device.getQueueFamilyProperties();

            for (uint32_t i = 0; i < queue_family_properties.size(); i++) {
                const auto &props = queue_family_properties[i];
                if ((!graphics.has_value() || (sparseGraphics == false && present.value_or(UINT32_MAX) != graphics.value())) && props.queueFlags & vk::QueueFlagBits::eGraphics) {
                    // we basically want to take the graphics queue when either: a) we dont have one yet. b) the current one is not sparse AND not the same as the present family.
                    graphics       = i;
                    sparseGraphics = (props.queueFlags & vk::QueueFlagBits::eSparseBinding) == vk::QueueFlagBits::eSparseBinding;
                }

                if (!present.has_value() && m_physical_device.getSurfaceSupportKHR(i, m_surface)) {
                    present = i;
                }

                if ((!transfer.has_value() || (!sparseTransfer && props.queueFlags & vk::QueueFlagBits::eSparseBinding)) && !(props.queueFlags & vk::QueueFlagBits::eGraphics) &&
                    props.queueFlags & vk::QueueFlagBits::eTransfer) {
                    // we basically are looking for a non-graphics, sparse transfer queue. once we find a non graphics transfer queue we take it, but if it's not sparse we keep
                    // looking for a sparse one we do this so we can find the optimized transfer hardware that many gpus have (but isn't present for the multipurpose queues)
                    sparseTransfer = (props.queueFlags & vk::QueueFlagBits::eSparseBinding) == vk::QueueFlagBits::eSparseBinding;
                    transfer       = i;
                }

                if ((!compute.has_value() || (!sparseCompute && props.queueFlags & vk::QueueFlagBits::eSparseBinding) ||
                     (!computeGraphicsShared && props.queueFlags & vk::QueueFlagBits::eGraphics)) &&
                    props.queueFlags & vk::QueueFlagBits::eCompute) {
                    // sparse is better than not still so we look for that (also, we look for a queue which can do compute and graphics because that is nice to have, and must
                    // exist).
                    compute               = i;
                    sparseCompute         = (props.queueFlags & vk::QueueFlagBits::eSparseBinding) == vk::QueueFlagBits::eSparseBinding;
                    computeGraphicsShared = (props.queueFlags & vk::QueueFlagBits::eGraphics) == vk::QueueFlagBits::eGraphics;
                }
            }

            if (!graphics.has_value()) {
                throw std::runtime_error("No graphics queue available");
            }

            if (!present.has_value()) {
                throw std::runtime_error("No queue supports presentation operations to this surface");
            }

            if (!transfer.has_value()) {
                transfer = graphics;
            }

            if (!compute.has_value()) {
                throw std::runtime_error("No compute queue available (this implies an off-spec driver. See implementation requirements noted in the info of VkQueueFlagBits about "
                                         "the requirements when graphics operations are supported).");
            }

            spdlog::info("Graphics Family: {}", graphics.value());
            spdlog::info("Present Family: {}", present.value());
            spdlog::info("Transfer Family: {}", transfer.value());
            spdlog::info("Compute Family: {}", compute.value());

            m_queue_families = {.graphics = graphics.value(), .present = present.value(), .transfer = transfer.value(), .compute = compute.value()};
        }

        struct {
            // any not listed here are supported by core 1.3
            bool maintenance5               = false;
            bool memory_budget              = false;
            bool memory_priority            = false;
            bool amd_device_coherent_memory = false;
        } vma_extension_support;

        {
            std::unordered_set<uint32_t> queue_families;
            queue_families.insert(m_queue_families.graphics);
            queue_families.insert(m_queue_families.present);
            queue_families.insert(m_queue_families.transfer);
            queue_families.insert(m_queue_families.compute);

            std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
            queue_create_infos.reserve(queue_families.size());
            std::array<float, 1> queue_priorities = {1.0f};

            for (const uint32_t &queue_family : queue_families) {
                queue_create_infos.emplace_back(vk::DeviceQueueCreateFlags{}, queue_family, queue_priorities);
            }

            std::vector<const char *> device_extensions = {
                VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            };

            for (auto available_extensions = m_physical_device.enumerateDeviceExtensionProperties(); const auto &extension : available_extensions) {
                if (strcmp(extension.extensionName, VK_KHR_MAINTENANCE_5_EXTENSION_NAME) == 0) {
                    device_extensions.push_back(VK_KHR_MAINTENANCE_5_EXTENSION_NAME);
                    vma_extension_support.maintenance5 = true;
                }

                if (strcmp(extension.extensionName, VK_EXT_MEMORY_BUDGET_EXTENSION_NAME) == 0) {
                    device_extensions.push_back(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
                    vma_extension_support.memory_budget = true;
                }

                if (strcmp(extension.extensionName, VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME) == 0) {
                    device_extensions.push_back(VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME);
                    vma_extension_support.memory_priority = true;
                }

                if (strcmp(extension.extensionName, VK_AMD_DEVICE_COHERENT_MEMORY_EXTENSION_NAME) == 0) {
                    device_extensions.push_back(VK_AMD_DEVICE_COHERENT_MEMORY_EXTENSION_NAME);
                    vma_extension_support.amd_device_coherent_memory = true;
                }
            }

            vk::PhysicalDeviceFeatures2        f2{};
            vk::PhysicalDeviceVulkan11Features v11f{};
            vk::PhysicalDeviceVulkan12Features v12f{};
            vk::PhysicalDeviceVulkan13Features v13f{};
            v13f.dynamicRendering = true;
            v13f.maintenance4     = true;
            v13f.synchronization2 = true;

            f2.pNext   = &v11f;
            v11f.pNext = &v12f;
            v12f.pNext = &v13f;

            m_device = m_physical_device.createDevice(vk::DeviceCreateInfo({}, queue_create_infos, {}, device_extensions, nullptr, &f2));
            VULKAN_HPP_DEFAULT_DISPATCHER.init(m_device);
        }

        {
            VmaVulkanFunctions vk_functions{};
            vk_functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
            vk_functions.vkGetDeviceProcAddr   = vkGetDeviceProcAddr;

            VmaAllocatorCreateInfo allocator_create_info{};
            allocator_create_info.flags = VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT | VMA_ALLOCATOR_CREATE_KHR_BIND_MEMORY2_BIT |
                VMA_ALLOCATOR_CREATE_KHR_MAINTENANCE4_BIT | VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
            if (vma_extension_support.maintenance5)
                allocator_create_info.flags |= VMA_ALLOCATOR_CREATE_KHR_MAINTENANCE5_BIT;
            if (vma_extension_support.memory_budget)
                allocator_create_info.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
            if (vma_extension_support.memory_priority)
                allocator_create_info.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT;
            if (vma_extension_support.amd_device_coherent_memory)
                allocator_create_info.flags |= VMA_ALLOCATOR_CREATE_AMD_DEVICE_COHERENT_MEMORY_BIT;
            allocator_create_info.vulkanApiVersion = vk::ApiVersion13;
            allocator_create_info.physicalDevice   = m_physical_device;
            allocator_create_info.device           = m_device;
            allocator_create_info.instance         = m_instance;
            allocator_create_info.pVulkanFunctions = &vk_functions;

            vmaCreateAllocator(&allocator_create_info, &m_allocator);
        }

        m_queues.graphics = m_device.getQueue(m_queue_families.graphics, 0);
        m_queues.present  = m_device.getQueue(m_queue_families.present, 0);
        m_queues.transfer = m_device.getQueue(m_queue_families.transfer, 0);
        m_queues.compute  = m_device.getQueue(m_queue_families.compute, 0);

        spdlog::info("Vulkan init complete");

        m_image_available_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
        m_render_finished_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
        m_in_flight_fences.resize(MAX_FRAMES_IN_FLIGHT);
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            m_image_available_semaphores[i] = m_device.createSemaphore({});
            m_render_finished_semaphores[i] = m_device.createSemaphore({});
            m_in_flight_fences[i]           = m_device.createFence({vk::FenceCreateFlagBits::eSignaled});
        }

        configure_swapchain(window);

        m_graphics_pool = m_device.createCommandPool(vk::CommandPoolCreateInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, m_queue_families.graphics));
        m_compute_pool  = m_device.createCommandPool(vk::CommandPoolCreateInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, m_queue_families.transfer));
        m_transfer_pool = m_device.createCommandPool(vk::CommandPoolCreateInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, m_queue_families.compute));
    }

    RenderContext::~RenderContext() {
        m_device.waitIdle();

        m_device.destroy(m_graphics_pool);
        m_device.destroy(m_transfer_pool);
        m_device.destroy(m_compute_pool);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            m_device.destroy(m_image_available_semaphores[i]);
            m_device.destroy(m_render_finished_semaphores[i]);
            m_device.destroy(m_in_flight_fences[i]);
        }

        for (const auto &view : m_swapchain_image_views) {
            m_device.destroy(view);
        }
        m_swapchain_image_views.clear();

        m_device.destroy(m_swapchain);

        vmaDestroyAllocator(m_allocator);
        m_device.destroy();
        m_instance.destroy(m_surface);
        m_instance.destroy(m_debug_messenger);
        m_instance.destroy();
    }

    void RenderContext::configure_swapchain(GLFWwindow *window) {
        const auto caps          = m_physical_device.getSurfaceCapabilitiesKHR(m_surface);
        const auto present_modes = m_physical_device.getSurfacePresentModesKHR(m_surface);
        const auto formats       = m_physical_device.getSurfaceFormatsKHR(m_surface);

        vk::SwapchainCreateInfoKHR create_info{};
        create_info.oldSwapchain = m_swapchain;

        vk::SurfaceFormatKHR surface_format = formats[0];
        for (const auto &format : formats) {
            if (format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
                surface_format = format;
                break;
            }
        }

        create_info.imageFormat     = surface_format.format;
        create_info.imageColorSpace = surface_format.colorSpace;

        create_info.presentMode = vk::PresentModeKHR::eFifo;
        for (const auto &mode : present_modes) {
            if (mode == vk::PresentModeKHR::eMailbox) {
                create_info.presentMode = mode;
            }
        }

        create_info.minImageCount = caps.minImageCount + 1;
        if (caps.maxImageCount > 0 && create_info.minImageCount > caps.maxImageCount) {
            create_info.minImageCount = caps.maxImageCount;
        }

        if (caps.currentExtent.width != UINT32_MAX) {
            create_info.imageExtent = caps.currentExtent;
        } else {
            int w, h;
            glfwGetFramebufferSize(window, &w, &h);

            create_info.imageExtent = vk::Extent2D{
                std::clamp<uint32_t>(w, caps.minImageExtent.width, caps.maxImageExtent.width),
                std::clamp<uint32_t>(h, caps.minImageExtent.height, caps.maxImageExtent.height),
            };
        }

        create_info.imageArrayLayers = 1;
        create_info.imageUsage       = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst;

        std::vector<uint32_t> qfs;
        if (m_queue_families.graphics != m_queue_families.present) {
            create_info.imageSharingMode = vk::SharingMode::eConcurrent;
            qfs                          = {m_queue_families.graphics, m_queue_families.present};
            create_info.setQueueFamilyIndices(qfs);
        } else {
            create_info.imageSharingMode = vk::SharingMode::eExclusive;
        }

        create_info.preTransform   = caps.currentTransform;
        create_info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
        create_info.clipped        = true;
        create_info.surface        = m_surface;

        m_swapchain = m_device.createSwapchainKHR(create_info);

        if (create_info.oldSwapchain) {
            m_device.waitIdle(); // wait for the device to be done with all the current things its doing so we don't step on any toes.
            for (const auto &view : m_swapchain_image_views) {
                m_device.destroy(view);
            }
            m_swapchain_image_views.clear();
            m_device.destroy(create_info.oldSwapchain);
        }

        m_swapchain_images = m_device.getSwapchainImagesKHR(m_swapchain);

        m_swapchain_image_views.reserve(m_swapchain_images.size());
        m_swapchain_image_last_layout.resize(m_swapchain_images.size());
        m_swapchain_image_last_owner.resize(m_swapchain_images.size());
        for (size_t i = 0; const auto &image : m_swapchain_images) {
            m_swapchain_image_views.push_back(m_device.createImageView(
                vk::ImageViewCreateInfo(vk::ImageViewCreateFlags{}, image, vk::ImageViewType::e2D, create_info.imageFormat,
                                        vk::ComponentMapping(vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA),
                                        vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1))));

            m_swapchain_image_last_layout[i] = vk::ImageLayout::eUndefined;
            m_swapchain_image_last_owner[i]  = m_queue_families.graphics;

            i++;
        }

        m_swapchain_configuration.format      = create_info.imageFormat;
        m_swapchain_configuration.color_space = create_info.imageColorSpace;
        m_swapchain_configuration.extent      = create_info.imageExtent;

        spdlog::info("Swapchain created with {} images", m_swapchain_images.size());

        m_swapchain_reloaded = true;
    }

    void RenderContext::render_frame(GLFWwindow *window, const std::function<void(const FrameInfo &info)> &f) {
        m_frame_info.current_frame   = m_current_frame;
        m_frame_info.in_flight       = m_in_flight_fences[m_current_frame];
        m_frame_info.image_available = m_image_available_semaphores[m_current_frame];
        m_frame_info.render_finished = m_render_finished_semaphores[m_current_frame];

        if (m_device.waitForFences(m_frame_info.in_flight, true, UINT64_MAX) != vk::Result::eSuccess) {
            throw std::runtime_error(
                "something bad"); // something bad happened probably because the only options here should be success or timeout, and if we timeout on 2^64 thats worrying.
        }

        try {
            auto r = m_device.acquireNextImageKHR(m_swapchain, UINT64_MAX, m_frame_info.image_available);
            if (r.result != vk::Result::eSuccess) {
                configure_swapchain(window);
                r = m_device.acquireNextImageKHR(m_swapchain, UINT64_MAX, m_frame_info.image_available);
            }

            m_frame_info.image_index = r.value;
        } catch (vk::OutOfDateKHRError &err) {
            configure_swapchain(window);
            m_frame_info.image_index = m_device.acquireNextImageKHR(m_swapchain, UINT64_MAX, m_frame_info.image_available).value;
            return;
        }

        m_frame_info.image              = m_swapchain_images[m_frame_info.image_index];
        m_frame_info.image_view         = m_swapchain_image_views[m_frame_info.image_index];
        m_frame_info.initial_owner      = m_swapchain_image_last_owner[m_frame_info.image_index];
        m_frame_info.swapchain_reloaded = m_swapchain_reloaded;
        m_swapchain_reloaded            = false;

        m_device.resetFences(m_frame_info.in_flight);

        f(m_frame_info);

        try {
            auto _ = m_queues.present.presentKHR(vk::PresentInfoKHR(m_frame_info.render_finished, m_swapchain, m_frame_info.image_index));
        } catch (vk::OutOfDateKHRError &err) {
            configure_swapchain(window);
        }

        m_current_frame = (m_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    std::vector<vk::CommandBuffer> RenderContext::create_graphics_command_buffers(const uint32_t count) const {
        return m_device.allocateCommandBuffers(vk::CommandBufferAllocateInfo(m_graphics_pool, vk::CommandBufferLevel::ePrimary, count));
    }

    void RenderContext::simple_rendering_start_transition(const vk::CommandBuffer cmd, const vk::Image image, const uint32_t initial_owner) const {
        vk::ImageMemoryBarrier imb{};
        imb.image               = image;
        imb.subresourceRange    = vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
        imb.oldLayout           = vk::ImageLayout::eUndefined;
        imb.newLayout           = vk::ImageLayout::eColorAttachmentOptimal;
        imb.srcAccessMask       = vk::AccessFlagBits::eNone;
        imb.dstAccessMask       = vk::AccessFlagBits::eColorAttachmentWrite;
        imb.srcQueueFamilyIndex = initial_owner;
        imb.dstQueueFamilyIndex = m_queue_families.graphics;
        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eColorAttachmentOutput, {}, {}, {}, imb);
    }

    void RenderContext::simple_rendering_end_transition(const vk::CommandBuffer cmd, const vk::Image image) const {
        vk::ImageMemoryBarrier imb{};
        imb.image               = image;
        imb.subresourceRange    = vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
        imb.oldLayout           = vk::ImageLayout::eColorAttachmentOptimal;
        imb.newLayout           = vk::ImageLayout::ePresentSrcKHR;
        imb.srcAccessMask       = vk::AccessFlagBits::eColorAttachmentWrite;
        imb.dstAccessMask       = vk::AccessFlagBits::eNone;
        imb.srcQueueFamilyIndex = m_queue_families.graphics;
        imb.srcQueueFamilyIndex = m_queue_families.present;
        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eBottomOfPipe, {}, {}, {}, imb);
    }

    void RenderContext::submit_for_rendering(vk::CommandBuffer cmd, const FrameInfo &frame_info) const {
        constexpr vk::PipelineStageFlags dst_stage = vk::PipelineStageFlagBits::eTopOfPipe;
        m_queues.graphics.submit(vk::SubmitInfo(frame_info.image_available, dst_stage, cmd, frame_info.render_finished), frame_info.in_flight);
    }

    vk::Rect2D RenderContext::swapchain_area() const {
        return vk::Rect2D{vk::Offset2D{0, 0}, m_swapchain_configuration.extent};
    }

    vk::Viewport RenderContext::swapchain_viewport(const float min_depth, const float max_depth) const {
        return vk::Viewport(0.0f, 0.0f, static_cast<float>(m_swapchain_configuration.extent.width), static_cast<float>(m_swapchain_configuration.extent.height), min_depth, max_depth);
    }

    void RenderContext::setup_validation_logger() {
        static bool configured = false;

        if (!configured) {
            configured        = true;
            const auto logger = spdlog::stdout_color_mt("validation");
            logger->set_level(spdlog::level::debug);
        }
    }

    VkBool32 RenderContext::validation_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT message_type,
                                                const VkDebugUtilsMessengerCallbackDataEXT *callback_data, void *user_data) {
        spdlog::level::level_enum level;
        switch (severity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            level = spdlog::level::debug;
            break;

        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            level = spdlog::level::info;
            break;

        default:
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            level = spdlog::level::warn;
            break;

        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            level = spdlog::level::err;
            break;
        }

        spdlog::get("validation")->log(level, "{} - {}", vk::to_string(static_cast<vk::DebugUtilsMessageTypeFlagsEXT>(message_type)), callback_data->pMessage);
        return VK_FALSE;
    }

    void record_single_use_commands(const vk::CommandBuffer &cmd, const std::function<void(const vk::CommandBuffer &)> &f, const bool reset) {
        if (reset)
            cmd.reset();
        cmd.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
        f(cmd);
        cmd.end();
    }

    vk::ShaderModule RenderContext::load_shader_module(const std::filesystem::path &path, const SourceType source_type) const {
        switch (source_type) {
        case SourceType::GLSL: {
            std::ifstream f(path, std::ios::in | std::ios::ate);
            if (!f.is_open()) {
                throw std::runtime_error("Failed to open file '" + path.string() + "'.");
            }

            const auto size = f.tellg();
            const auto buf  = new char[size];
            std::memset(buf, 0, size);
            f.seekg(0, std::ios::beg);
            f.read(buf, size);
            f.close();

            const std::string text = buf;
            delete[] buf;

            return compile_glsl_shader(text, path.string());
        }
        case SourceType::SPIRV: {
            std::ifstream f(path, std::ios::in | std::ios::ate | std::ios::binary);
            if (!f.is_open()) {
                throw std::runtime_error("Failed to open file '" + path.string() + "'.");
            }

            const std::size_t     size = f.tellg() / 4;
            std::vector<uint32_t> code(size);
            f.seekg(0, std::ios::beg);
            f.read(reinterpret_cast<char *>(code.data()), static_cast<std::streamsize>(size * sizeof(uint32_t)));
            f.close();

            return load_spirv_shader(code);
        }
        default:
            throw std::runtime_error("Unknown source type.");
        }
    }

    vk::ShaderModule RenderContext::compile_glsl_shader(const std::string &source, const std::string& filename) const {
        const shaderc::Compiler       compiler;
        const shaderc::CompileOptions options;

        const auto result = compiler.CompileGlslToSpv(source, shaderc_glsl_infer_from_source, filename.c_str(), options);
        if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
            spdlog::critical("Failed to compiler shader: {}", result.GetErrorMessage());
            throw std::runtime_error("Failed to compile shader.");
        }

        return load_spirv_shader(std::vector(result.cbegin(), result.cend()));
    }

    vk::ShaderModule RenderContext::load_spirv_shader(const std::vector<uint32_t> &code) const {
        return m_device.createShaderModule(vk::ShaderModuleCreateInfo({}, code));
    }

} // namespace vke
