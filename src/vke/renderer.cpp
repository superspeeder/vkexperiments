#include "renderer.hpp"

namespace vke {
    GraphicsPipeline::GraphicsPipeline(vk::Device device, const GraphicsPipelineBuilder &builder, vk::PipelineCache cache) : m_device(device) {
        vk::GraphicsPipelineCreateInfo create_info{};

        std::vector<vk::PipelineShaderStageCreateInfo> stages;
        stages.reserve(builder.stages.size());
        for (const auto &[stage, entry_point, module] : builder.stages) {
            stages.emplace_back(vk::PipelineShaderStageCreateFlags{}, stage, module, entry_point.c_str());
        }
        create_info.setStages(stages);

        vk::PipelineDynamicStateCreateInfo dynamic_state{};
        dynamic_state.setDynamicStates(builder.dynamic_states);
        create_info.setPDynamicState(&dynamic_state);

        vk::PipelineVertexInputStateCreateInfo           vertex_input_state{};
        std::vector<vk::VertexInputBindingDescription>   vertex_bindings;
        std::vector<vk::VertexInputAttributeDescription> vertex_attributes;

        vertex_bindings.reserve(builder.vertex_buffer_bindings.size());
        for (const auto &[binding, stride, input_rate, attributes] : builder.vertex_buffer_bindings) {
            vertex_bindings.emplace_back(binding, stride, input_rate);
            for (const auto &[location, format, offset] : attributes) {
                vertex_attributes.emplace_back(location, binding, format, offset);
            }
        }

        vertex_input_state.setVertexBindingDescriptions(vertex_bindings);
        vertex_input_state.setVertexAttributeDescriptions(vertex_attributes);
        create_info.setPVertexInputState(&vertex_input_state);

        vk::PipelineInputAssemblyStateCreateInfo input_assembly{};
        input_assembly.setTopology(builder.topology);
        input_assembly.setPrimitiveRestartEnable(builder.enable_primitive_restart);
        create_info.setPInputAssemblyState(&input_assembly);

        vk::PipelineViewportStateCreateInfo viewport_state{};
        viewport_state.setViewports(builder.viewports);
        viewport_state.setScissors(builder.scissors);
        create_info.setPViewportState(&viewport_state);

        vk::PipelineRasterizationStateCreateInfo rasterization_state{};
        rasterization_state.setDepthClampEnable(builder.enable_depth_clamp);
        rasterization_state.setRasterizerDiscardEnable(builder.discard_rasterizer_output);
        rasterization_state.setPolygonMode(builder.polygon_mode);
        rasterization_state.setCullMode(builder.cull_mode);
        rasterization_state.setFrontFace(builder.front_face);
        rasterization_state.setLineWidth(builder.line_width);
        rasterization_state.setDepthBiasEnable(builder.depth_bias.has_value());
        if (builder.depth_bias.has_value()) {
            rasterization_state.setDepthBiasConstantFactor(builder.depth_bias->constant_factor);
            rasterization_state.setDepthBiasClamp(builder.depth_bias->clamp);
            rasterization_state.setDepthBiasSlopeFactor(builder.depth_bias->slope_factor);
        }
        create_info.setPRasterizationState(&rasterization_state);

        vk::PipelineMultisampleStateCreateInfo multisample_state{};
        multisample_state.setSampleShadingEnable(builder.enable_sample_shading);
        multisample_state.setRasterizationSamples(builder.rasterization_samples);
        multisample_state.setMinSampleShading(builder.min_sample_shading);
        multisample_state.setPSampleMask(builder.sample_mask.data());
        multisample_state.setAlphaToCoverageEnable(builder.enable_alpha_to_coverage);
        multisample_state.setAlphaToOneEnable(builder.enable_alpha_to_one);
        create_info.setPMultisampleState(&multisample_state);

        std::vector<vk::PipelineColorBlendAttachmentState> color_attachments;
        color_attachments.reserve(builder.color_blend_attachments.size());
        for (const auto &[color_write_mask, enable_blending, color, alpha] : builder.color_blend_attachments) {
            color_attachments.emplace_back(enable_blending, color.src, color.dst, color.op, alpha.src, alpha.dst, alpha.op, color_write_mask);
        }
        vk::PipelineColorBlendStateCreateInfo color_blend_state{};
        color_blend_state.setAttachments(color_attachments);
        color_blend_state.setLogicOpEnable(builder.logic_op.has_value());
        color_blend_state.setLogicOp(builder.logic_op.value_or(vk::LogicOp::eCopy));
        color_blend_state.setBlendConstants(builder.blend_constants);
        create_info.setPColorBlendState(&color_blend_state);

        create_info.setLayout(builder.layout);

        if (builder.render_pass.has_value()) {
            auto [render_pass, subpass_index] = builder.render_pass.value();
            create_info.setRenderPass(render_pass);
            create_info.setSubpass(subpass_index);
        }

        vk::PipelineRenderingCreateInfo rendering_info{};
        if (builder.dynamic_rendering_info.has_value()) {
            rendering_info.setColorAttachmentFormats(builder.dynamic_rendering_info->color_formats);
            rendering_info.setDepthAttachmentFormat(builder.dynamic_rendering_info->depth_format);
            rendering_info.setStencilAttachmentFormat(builder.dynamic_rendering_info->stencil_format);
            rendering_info.setViewMask(builder.dynamic_rendering_info->view_mask);
            create_info.setPNext(&rendering_info);
        }

        m_pipeline = m_device.createGraphicsPipeline(cache, create_info).value;
    }

    GraphicsPipeline::~GraphicsPipeline() {
        m_device.destroy(m_pipeline);
    }

    void ActiveRenderer::bind_graphics_pipeline(const vk::Pipeline pipeline) const {
        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
    }

    void ActiveRenderer::bind_graphics_pipeline(const GraphicsPipeline *pipeline) const {
        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline->get());
    }

    void ActiveRenderer::bind_graphics_pipeline(const std::shared_ptr<GraphicsPipeline> &pipeline) const {
        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline->get());
    }

    void ActiveRenderer::bind_graphics_pipeline(const std::unique_ptr<GraphicsPipeline> &pipeline) const {
        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline->get());
    }

    void ActiveRenderer::bind_graphics_pipeline(const GraphicsPipeline &pipeline) const {
        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.get());
    }

    void ActiveRenderer::bind_compute_pipeline(const vk::Pipeline pipeline) const {
        cmd.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline);
    }

    void ActiveRenderer::bind_mesh(const std::unique_ptr<Mesh> &mesh) const {
        cmd.bindVertexBuffers(0, mesh->vertex_buffer().buffer, 0ULL);
        if (mesh->index_buffer().has_value()) {
            cmd.bindIndexBuffer(mesh->index_buffer().value().buffer, 0, mesh->index_type());
        }
    }

    void ActiveRenderer::bind_mesh(const std::shared_ptr<Mesh> &mesh) const {
        cmd.bindVertexBuffers(0, mesh->vertex_buffer().buffer, 0ULL);
        if (mesh->index_buffer().has_value()) {
            cmd.bindIndexBuffer(mesh->index_buffer().value().buffer, 0, mesh->index_type());
        }
    }

    void ActiveRenderer::bind_mesh(const Mesh *mesh) const {
        cmd.bindVertexBuffers(0, mesh->vertex_buffer().buffer, 0ULL);
        if (mesh->index_buffer().has_value()) {
            cmd.bindIndexBuffer(mesh->index_buffer().value().buffer, 0, mesh->index_type());
        }
    }

    void SimpleRenderer::render(const vk::CommandBuffer &cmd, const vk::ImageView view, const vk::Rect2D &render_area, const std::function<void(ActiveRenderer &&)> &f) const {
        const vk::ClearColorValue clear_color(m_clear_color.r, m_clear_color.g, m_clear_color.b, m_clear_color.a);

        vk::RenderingAttachmentInfo color{
            view,
            vk::ImageLayout::eColorAttachmentOptimal,
            vk::ResolveModeFlagBits::eNone,
            VK_NULL_HANDLE,
            vk::ImageLayout::eUndefined,
            vk::AttachmentLoadOp::eClear,
            vk::AttachmentStoreOp::eStore,
            clear_color,
        };

        cmd.beginRendering(vk::RenderingInfo({}, render_area, 1, 0, color));
        f(ActiveRenderer(cmd));
        cmd.endRendering();
    }
} // namespace vke
