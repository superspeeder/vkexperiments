#pragma once

#include <filesystem>
#include <functional>
#include <vulkan/vulkan.hpp>

#include <glm/glm.hpp>

#include <optional>

#include "vke/mesh.hpp"

namespace vke {
    struct VertexBufferAttribute {
        uint32_t   location;
        vk::Format format;
        uint32_t   offset;
    };

    struct VertexBufferBinding {
        uint32_t                           binding, stride;
        vk::VertexInputRate                input_rate;
        std::vector<VertexBufferAttribute> attributes;
    };

    struct DepthBias {
        float constant_factor = 0.0f;
        float clamp           = 0.0f;
        float slope_factor    = 0.0f;
    };

    struct BlendFunction {
        vk::BlendFactor src, dst;
        vk::BlendOp     op;
    };

    namespace blending {
        static constexpr BlendFunction SOURCE_ONLY{vk::BlendFactor::eOne, vk::BlendFactor::eZero, vk::BlendOp::eAdd};
        static constexpr BlendFunction ALPHA_BLENDING{vk::BlendFactor::eSrcAlpha, vk::BlendFactor::eOneMinusSrcAlpha, vk::BlendOp::eAdd};
    } // namespace blending

    struct ColorBlendAttachment {
        vk::ColorComponentFlags color_write_mask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB |
            vk::ColorComponentFlagBits::eA;
        bool          enable_blending = true;
        BlendFunction color           = blending::ALPHA_BLENDING;
        BlendFunction alpha           = blending::SOURCE_ONLY;
    };

    struct ShaderStage {
        vk::ShaderStageFlagBits stage;
        std::string             entry_point;
        vk::ShaderModule        module;
    };

    struct DynamicRenderingInfo {
        std::vector<vk::Format> color_formats;
        vk::Format              depth_format   = vk::Format::eUndefined;
        vk::Format              stencil_format = vk::Format::eUndefined;
        uint32_t                view_mask      = 0;
    };

    struct GraphicsPipelineBuilder {
        std::vector<ShaderStage>         stages;
        std::vector<vk::DynamicState>    dynamic_states;
        std::vector<VertexBufferBinding> vertex_buffer_bindings;
        vk::PrimitiveTopology            topology                 = vk::PrimitiveTopology::eTriangleList;
        bool                             enable_primitive_restart = false;
        std::vector<vk::Viewport>        viewports;
        std::vector<vk::Rect2D>          scissors;
        bool                             enable_depth_clamp        = false;
        bool                             discard_rasterizer_output = false;
        vk::PolygonMode                  polygon_mode              = vk::PolygonMode::eFill;
        vk::CullModeFlags                cull_mode                 = vk::CullModeFlagBits::eBack;
        vk::FrontFace                    front_face                = vk::FrontFace::eClockwise;
        float                            line_width                = 1.0f;
        std::optional<DepthBias>         depth_bias                = std::nullopt;
        bool                             enable_sample_shading     = false;
        vk::SampleCountFlagBits          rasterization_samples     = vk::SampleCountFlagBits::e1;
        float                            min_sample_shading        = 1.0f;
        std::vector<vk::SampleMask>      sample_mask;
        bool                             enable_alpha_to_coverage = false;
        bool                             enable_alpha_to_one      = false;
        // TODO: depth stencil stuff
        std::vector<ColorBlendAttachment> color_blend_attachments;
        std::optional<vk::LogicOp>        logic_op = std::nullopt;
        std::array<float, 4>              blend_constants;
        vk::PipelineLayout                layout;

        std::optional<DynamicRenderingInfo> dynamic_rendering_info = std::nullopt;

        std::optional<std::pair<vk::RenderPass, uint32_t>> render_pass = std::nullopt;
    };

    class GraphicsPipeline {
      public:
        GraphicsPipeline(vk::Device device, const GraphicsPipelineBuilder &builder, vk::PipelineCache cache = VK_NULL_HANDLE);

        ~GraphicsPipeline();

        [[nodiscard]] inline vk::Pipeline get() const { return m_pipeline; };

      private:
        vk::Device   m_device;
        vk::Pipeline m_pipeline;
    };

    class ActiveRenderer {
      public:
        inline explicit ActiveRenderer(const vk::CommandBuffer &cmd_) : cmd(cmd_){};

        inline vk::CommandBuffer *operator->() { return &cmd; }

        inline const vk::CommandBuffer *operator->() const { return &cmd; }

        [[nodiscard]] inline vk::CommandBuffer get() const { return cmd; }

        inline explicit operator vk::CommandBuffer() const { return cmd; }

        inline const vk::CommandBuffer &operator*() const { return cmd; }

        void bind_graphics_pipeline(vk::Pipeline pipeline) const;
        void bind_graphics_pipeline(const GraphicsPipeline *pipeline) const;
        void bind_graphics_pipeline(const std::shared_ptr<GraphicsPipeline> &pipeline) const;
        void bind_graphics_pipeline(const std::unique_ptr<GraphicsPipeline> &pipeline) const;
        void bind_graphics_pipeline(const GraphicsPipeline &pipeline) const;
        void bind_compute_pipeline(vk::Pipeline pipeline) const;

        void bind_mesh(const std::unique_ptr<Mesh> &mesh) const;
        void bind_mesh(const std::shared_ptr<Mesh> &mesh) const;
        void bind_mesh(const Mesh *mesh) const;

      private:
        vk::CommandBuffer cmd;
    };

    class SimpleRenderer {
      public:
        void render(const vk::CommandBuffer &cmd, vk::ImageView view, const vk::Rect2D &render_area, const std::function<void(ActiveRenderer &&)> &f) const;

        [[nodiscard]] glm::vec4 clear_color() const { return m_clear_color; }

        void set_clear_color(const glm::vec4 &clear_color) { m_clear_color = clear_color; }

      private:
        glm::vec4 m_clear_color = {0.0f, 0.0f, 0.0f, 1.0f};
    };
} // namespace vke
