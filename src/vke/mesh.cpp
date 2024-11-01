#include "mesh.hpp"

namespace vke {
    Mesh::Mesh(const std::shared_ptr<RenderContext> &rc, const size_t vertex_data_size, const void *vertex_data, const size_t index_data_size, const void *index_data,
               const MeshType type, const ExtraMeshSettings &extra_mesh_settings)
        : m_rc(rc), m_index_type(extra_mesh_settings.index_type) {

        const MemoryUsage vertex_mu = type == MeshType::Static ? MemoryUsage::DeviceOnly : MemoryUsage::Auto;
        const MemoryUsage index_mu  = extra_mesh_settings.index_separate_type.value_or(type) == MeshType::Static ? MemoryUsage::DeviceOnly : MemoryUsage::Auto;

        m_vertex_buffer = m_rc->create_buffer(vertex_data_size, vertex_data, vertex_mu, extra_mesh_settings.vertex_usage_flags);

        if (index_data && index_data_size) {
            m_index_buffer = m_rc->create_buffer(index_data_size, index_data, index_mu, extra_mesh_settings.index_usage_flags);
        }
    }

    Mesh::~Mesh() {
        m_rc->destroy_buffer(m_vertex_buffer);

        if (m_index_buffer.has_value()) {
            m_rc->destroy_buffer(m_index_buffer.value());
        }
    }

    std::unique_ptr<Mesh> Mesh::create(const std::shared_ptr<RenderContext> &rc, const size_t vertex_data_size, const void *vertex_data, const size_t index_data_size,
                                       const void *index_data, const MeshType type, const ExtraMeshSettings &extra_mesh_settings) {
        return std::unique_ptr<Mesh>(new Mesh(rc, vertex_data_size, vertex_data, index_data_size, index_data, type, extra_mesh_settings));
    }

    std::unique_ptr<Mesh> Mesh::create(const std::shared_ptr<RenderContext> &rc, const size_t vertex_data_size, const void *vertex_data, const MeshType type,
                                       const ExtraMeshSettings &extra_mesh_settings) {
        return create(rc, vertex_data_size, vertex_data, 0, nullptr, type, extra_mesh_settings);
    }

    std::shared_ptr<Mesh> Mesh::create_shared(const std::shared_ptr<RenderContext> &rc, const size_t vertex_data_size, const void *vertex_data, const size_t index_data_size,
                                              const void *index_data, const MeshType type, const ExtraMeshSettings &extra_mesh_settings) {
        return std::shared_ptr<Mesh>(new Mesh(rc, vertex_data_size, vertex_data, index_data_size, index_data, type, extra_mesh_settings));
    }

    std::shared_ptr<Mesh> Mesh::create_shared(const std::shared_ptr<RenderContext> &rc, const size_t vertex_data_size, const void *vertex_data, const MeshType type,
                                              const ExtraMeshSettings &extra_mesh_settings) {
        return create_shared(rc, vertex_data_size, vertex_data, 0, nullptr, type, extra_mesh_settings);
    }


} // namespace vke
