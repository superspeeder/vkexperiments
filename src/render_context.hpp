#pragma once

#include <GLFW/glfw3.h>
#include <filesystem>
#include <glm/glm.hpp>
#include <shaderc/shaderc.hpp>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

#include <functional>

namespace vke {
    enum class SourceType {
        SPIRV,
        GLSL,
    };

    struct QueueSet {
        vk::Queue graphics, present, transfer, compute;
    };

    struct QueueFamilies {
        uint32_t graphics, present, transfer, compute;
    };

    struct SwapchainConfiguration {
        vk::Format        format;
        vk::ColorSpaceKHR color_space;
        vk::Extent2D      extent;
    };

    struct FrameInfo {
        uint32_t      image_index;
        uint32_t      current_frame;
        vk::Image     image;
        vk::ImageView image_view;
        vk::Semaphore image_available;
        vk::Semaphore render_finished;
        vk::Fence     in_flight;
        uint32_t      initial_owner;
        bool          swapchain_reloaded;
    };

    class RenderContext {
      public:
        static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

        explicit RenderContext(GLFWwindow *window);

        ~RenderContext();

        void configure_swapchain(GLFWwindow *window);

        void render_frame(GLFWwindow *window, const std::function<void(const FrameInfo &info)> &f);

        [[nodiscard]] std::vector<vk::CommandBuffer> create_graphics_command_buffers(uint32_t count) const;

        [[nodiscard]] vk::Instance instance() const { return m_instance; }

        [[nodiscard]] vk::SurfaceKHR surface() const { return m_surface; }

        [[nodiscard]] vk::PhysicalDevice physical_device() const { return m_physical_device; }

        [[nodiscard]] vk::Device device() const { return m_device; }

        [[nodiscard]] QueueSet queues() const { return m_queues; }

        [[nodiscard]] QueueFamilies queue_families() const { return m_queue_families; }

        [[nodiscard]] VmaAllocator allocator() const { return m_allocator; }

        [[nodiscard]] SwapchainConfiguration swapchain_configuration() const { return m_swapchain_configuration; }

        [[nodiscard]] vk::SwapchainKHR swapchain() const { return m_swapchain; }

        [[nodiscard]] std::vector<vk::Image> swapchain_images() const { return m_swapchain_images; }

        [[nodiscard]] std::vector<vk::ImageView> swapchain_image_views() const { return m_swapchain_image_views; }

        // these do extremely simple layout transitions
        void simple_rendering_start_transition(vk::CommandBuffer cmd, vk::Image image, uint32_t initial_owner) const;
        void simple_rendering_end_transition(vk::CommandBuffer cmd, vk::Image image) const;

        /**
         * @brief This is a function to ease submitting command buffers for simple render setups
         *
         * This function has some assumptions:
         * - The queue submission will wait on the image availability semaphore for this frame. <b>Do not tie commands to that which need to happen first, they might not.</b>
         * - The wait stage is always TOP_OF_PIPE.
         * - The submission will always use the render finished semaphore as the signal semaphore.
         * - The submission will always signal the in flight fence upon completion.
         *
         * If you need any more advanced behavior, use a different method for submitting commands.
         *
         * @param cmd The command buffer being submitted
         * @param frame_info The frame info for the frame being rendered
         */
        void submit_for_rendering(vk::CommandBuffer cmd, const FrameInfo &frame_info) const;

        [[nodiscard]] vk::Rect2D swapchain_area() const;
        [[nodiscard]] vk::Viewport swapchain_viewport(float min_depth = 0.0f, float max_depth = 1.0f) const;

        [[nodiscard]] vk::ShaderModule load_shader_module(const std::filesystem::path& path, SourceType source_type) const;
        [[nodiscard]] vk::ShaderModule compile_glsl_shader(const std::string& source, const std::string& filename) const;
        [[nodiscard]] vk::ShaderModule load_spirv_shader(const std::vector<uint32_t>& code) const;

        std::tuple<vk::Buffer, VmaAllocation, VmaAllocationInfo> create_buffer(size_t size, const void* data);

      private:
        vk::Instance               m_instance;
        vk::DebugUtilsMessengerEXT m_debug_messenger;
        vk::SurfaceKHR             m_surface;
        vk::PhysicalDevice         m_physical_device;
        vk::Device                 m_device;
        QueueSet                   m_queues;
        QueueFamilies              m_queue_families;
        VmaAllocator               m_allocator;

        SwapchainConfiguration m_swapchain_configuration;
        FrameInfo              m_frame_info;

        vk::SwapchainKHR             m_swapchain;
        std::vector<vk::Image>       m_swapchain_images;
        std::vector<vk::ImageView>   m_swapchain_image_views;
        std::vector<vk::ImageLayout> m_swapchain_image_last_layout;
        std::vector<uint32_t>        m_swapchain_image_last_owner;

        std::vector<vk::Semaphore> m_image_available_semaphores;
        std::vector<vk::Semaphore> m_render_finished_semaphores;
        std::vector<vk::Fence>     m_in_flight_fences;

        vk::CommandPool m_graphics_pool;
        vk::CommandPool m_transfer_pool;
        vk::CommandPool m_compute_pool;

        uint32_t m_current_frame      = 0;
        bool     m_swapchain_reloaded = false;

        static void setup_validation_logger();

        static VkBool32 VKAPI_CALL validation_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT message_type,
                                                       const VkDebugUtilsMessengerCallbackDataEXT *callback_data, void *user_data);
    };

    /**
     * @brief This function is a simple shortcut for recording commands into a buffer for single use purposes.
     *
     * This function records the commands with the ONE_TIME_SUBMIT flag set.
     *
     * @param cmd
     * @param f
     * @param reset whether to reset the command buffer before recording or not.
     */
    void record_single_use_commands(const vk::CommandBuffer &cmd, const std::function<void(const vk::CommandBuffer &)> &f, const bool reset = false);
} // namespace vke
