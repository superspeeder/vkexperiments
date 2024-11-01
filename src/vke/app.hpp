#pragma once
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

#include "vke/render_context.hpp"
#include "vke/renderer.hpp"
#include "vke/state_track.hpp"
#include "vke/mesh.hpp"

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

        std::unique_ptr<Mesh> m_mesh;

        void reload_image_tracking();

        std::vector<vk::CommandBuffer> m_command_buffers;
    };
} // namespace vke
