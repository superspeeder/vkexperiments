#pragma once
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

#include "render_context.hpp"
#include "renderer.hpp"
#include "state_track.hpp"

namespace vke {
    class App {
      public:
         App();
        ~App();

        void run();

        void render();

      private:
        GLFWwindow *m_window;

        std::shared_ptr<spdlog::logger> m_logger;
        std::shared_ptr<RenderContext>  m_render_context;
        std::shared_ptr<SimpleRenderer> m_simple_renderer;
        std::vector<TrackedImage>       m_tracked_images;

        vk::ShaderModule m_vertex_module;
        vk::ShaderModule m_fragment_module;

        vk::PipelineLayout                m_pipeline_layout;
        std::unique_ptr<GraphicsPipeline> m_pipeline;

        void reload_image_tracking();

        std::vector<vk::CommandBuffer> m_command_buffers;
    };
} // namespace vke
