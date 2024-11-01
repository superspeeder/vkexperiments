#pragma once

#include <optional>
#include <ranges>

#include "vke/render_context.hpp"

#include "vke/util.hpp"

namespace vke {


    enum class MeshType {
        Static,
        Dynamic, // dynamic will be mappable on the host, static will not. use accordingly
    };

    struct ExtraMeshSettings {
        vk::BufferUsageFlags    vertex_usage_flags = vk::BufferUsageFlagBits::eVertexBuffer;
        vk::BufferUsageFlags    index_usage_flags  = vk::BufferUsageFlagBits::eIndexBuffer;
        std::optional<MeshType> index_separate_type;
        vk::IndexType           index_type = vk::IndexType::eUint32;
    };

    class Mesh final {
        Mesh(const std::shared_ptr<RenderContext> &rc, size_t vertex_data_size, const void *vertex_data, size_t index_data_size, const void *index_data, MeshType type,
             const ExtraMeshSettings &extra_mesh_settings = {});

      public:
        ~Mesh();

        static std::unique_ptr<Mesh> create(const std::shared_ptr<RenderContext> &rc, size_t vertex_data_size, const void *vertex_data, size_t index_data_size,
                                            const void *index_data, MeshType type, const ExtraMeshSettings &extra_mesh_settings = {});
        static std::unique_ptr<Mesh> create(const std::shared_ptr<RenderContext> &rc, size_t vertex_data_size, const void *vertex_data, MeshType type,
                                            const ExtraMeshSettings &extra_mesh_settings = {});

        static std::shared_ptr<Mesh> create_shared(const std::shared_ptr<RenderContext> &rc, size_t vertex_data_size, const void *vertex_data, size_t index_data_size,
                                                   const void *index_data, MeshType type, const ExtraMeshSettings &extra_mesh_settings = {});
        static std::shared_ptr<Mesh> create_shared(const std::shared_ptr<RenderContext> &rc, size_t vertex_data_size, const void *vertex_data, MeshType type,
                                                   const ExtraMeshSettings &extra_mesh_settings = {});

        template <std::ranges::contiguous_range Range>
        inline static std::unique_ptr<Mesh> create(const std::shared_ptr<RenderContext> &rc, Range &&data, const MeshType type, const ExtraMeshSettings &extra_mesh_settings = {}) {
            return create(rc, byte_size(data), std::data(data), 0, nullptr, type, extra_mesh_settings);
        }

        template <std::ranges::contiguous_range Range>
        inline static std::shared_ptr<Mesh> create_shared(const std::shared_ptr<RenderContext> &rc, Range &&data, const MeshType type,
                                                          const ExtraMeshSettings &extra_mesh_settings = {}) {
            return create_shared(rc, byte_size(data), std::data(data), 0, nullptr, type, extra_mesh_settings);
        }

        template <std::ranges::contiguous_range Range, std::ranges::contiguous_range Range2>
        inline static std::unique_ptr<Mesh> create(const std::shared_ptr<RenderContext> &rc, Range &&data, Range2 &&index_data, const MeshType type,
                                                   const ExtraMeshSettings &extra_mesh_settings = {}) {
            return create(rc, byte_size(data), std::data(data), byte_size(index_data), std::data(index_data), type, extra_mesh_settings);
        }

        template <std::ranges::contiguous_range Range, std::ranges::contiguous_range Range2>
        inline static std::shared_ptr<Mesh> create_shared(const std::shared_ptr<RenderContext> &rc, Range &&data, Range2 &&index_data, const MeshType type,
                                                          const ExtraMeshSettings &extra_mesh_settings = {}) {
            return create_shared(rc, byte_size(data), std::data(data), byte_size(index_data), std::data(index_data), type, extra_mesh_settings);
        }

        [[nodiscard]] BufferInfo vertex_buffer() const { return m_vertex_buffer; }

        [[nodiscard]] std::optional<BufferInfo> index_buffer() const { return m_index_buffer; }

        [[nodiscard]] vk::IndexType index_type() const { return m_index_type; }

      private:
        std::shared_ptr<RenderContext> m_rc;
        BufferInfo                     m_vertex_buffer;
        std::optional<BufferInfo>      m_index_buffer;
        vk::IndexType                  m_index_type;
    };

} // namespace vke
