#include "app.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>

namespace vke {
    App::App() {
        auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        m_logger  = std::make_shared<spdlog::logger>("vke", sink);
        m_logger->set_level(spdlog::level::debug);
        spdlog::set_default_logger(m_logger);

        spdlog::info("Hello!");

        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        m_window = glfwCreateWindow(1000, 1000, "Hello!", nullptr, nullptr);
        spdlog::info("Created window");

        m_render_context = std::make_shared<RenderContext>(m_window);

        m_command_buffers = m_render_context->create_graphics_command_buffers(vke::RenderContext::MAX_FRAMES_IN_FLIGHT);

        glfwSetWindowUserPointer(m_window, this);

        m_simple_renderer = std::make_shared<SimpleRenderer>();
        m_simple_renderer->set_clear_color({0.0f, 1.0f, 0.0f, 1.0f});

        m_vertex_module   = m_render_context->load_shader_module("res/shader.vert", SourceType::GLSL);
        m_fragment_module = m_render_context->load_shader_module("res/shader.frag", SourceType::GLSL);

        m_pipeline_layout = m_render_context->device().createPipelineLayout({});

        GraphicsPipelineBuilder builder{};
        builder.vertex_buffer_bindings = {VertexBufferBinding{
            0,
            sizeof(glm::vec4),
            vk::VertexInputRate::eVertex,
            {
                VertexBufferAttribute{0, vk::Format::eR32G32Sfloat, 0},
                VertexBufferAttribute{1, vk::Format::eR32G32Sfloat, sizeof(float) * 2},
            },
        }};

        builder.stages = {
            ShaderStage{vk::ShaderStageFlagBits::eVertex, "main", m_vertex_module},
            ShaderStage{vk::ShaderStageFlagBits::eFragment, "main", m_fragment_module},
        };
        builder.dynamic_states = {
            vk::DynamicState::eViewport,
            vk::DynamicState::eScissor,
        };
        builder.viewports = {
            m_render_context->swapchain_viewport(),
        };
        builder.scissors = {
            m_render_context->swapchain_area(),
        };
        builder.color_blend_attachments = {
            ColorBlendAttachment{},
        };
        builder.dynamic_rendering_info = {.color_formats = {m_render_context->swapchain_configuration().format}};
        builder.layout                 = m_pipeline_layout;

        m_pipeline = std::make_unique<GraphicsPipeline>(m_render_context->device(), builder);

        std::vector<glm::vec4> vertices = {
            {-0.5f, 0.5f, -1.0f, -1.0f}, // bl
            {-0.5f, -0.5f, -1.0f, 1.0f}, // tl
            {0.5f, -0.5f, 1.0f, 1.0f},   // tr
            {0.5f, 0.5f, 1.0f, -1.0f},   // br
        };

        std::vector<uint16_t> indices = {0, 1, 2, 0, 2, 3};

        m_mesh = Mesh::create(m_render_context, vertices, indices, MeshType::Static, {.index_type = vk::IndexType::eUint16});
    }

    App::~App() {
        m_render_context->device().waitIdle();

        m_mesh.reset();

        m_pipeline.reset();
        m_render_context->device().destroy(m_vertex_module);
        m_render_context->device().destroy(m_fragment_module);
        m_render_context->device().destroy(m_pipeline_layout);

        m_render_context.reset();

        glfwDestroyWindow(m_window);
        glfwTerminate();

        spdlog::info("Goodbye!");
        spdlog::shutdown();
    }

    void App::run() {
        while (!glfwWindowShouldClose(m_window)) {
            glfwPollEvents();

            render();
        }
    }

    void App::render() {
        m_render_context->render_frame(m_window, [&](const FrameInfo &frame_info) {
            if (frame_info.swapchain_reloaded)
                reload_image_tracking();

            const auto &command_buffer = m_command_buffers[frame_info.current_frame];
            record_single_use_commands(
                command_buffer,
                [&](const vk::CommandBuffer &cmd) {
                    m_tracked_images[frame_info.image_index].set_layout(vk::ImageLayout::eUndefined);

                    m_tracked_images[frame_info.image_index].transition(cmd, vk::PipelineStageFlagBits2::eTopOfPipe, vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                                                                        vk::ImageLayout::eColorAttachmentOptimal, vk::AccessFlagBits2::eColorAttachmentWrite,
                                                                        m_render_context->queue_families().graphics);

                    m_simple_renderer->render(cmd, frame_info.image_view, m_render_context->swapchain_area(), [&](ActiveRenderer &&r) {
                        r.bind_graphics_pipeline(m_pipeline);
                        r->setViewport(0, m_render_context->swapchain_viewport());
                        r->setScissor(0, m_render_context->swapchain_area());
                        r.bind_mesh(m_mesh);
                        r->drawIndexed(6, 1, 0, 0, 0);
                    });

                    m_tracked_images[frame_info.image_index].transition(cmd, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::PipelineStageFlagBits2::eBottomOfPipe,
                                                                        vk::ImageLayout::ePresentSrcKHR, vk::AccessFlagBits2::eNone, m_render_context->queue_families().present);
                },
                true);


            m_render_context->submit_for_rendering(command_buffer, frame_info);
        });
    }

    void App::reload_image_tracking() {
        const auto &images = m_render_context->swapchain_images();
        m_tracked_images.clear();
        m_tracked_images.reserve(images.size());
        for (const auto &image : images) {
            m_tracked_images.emplace_back(image, vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));
        }
    }
} // namespace vke
