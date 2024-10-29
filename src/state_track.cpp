#include "state_track.hpp"

#include <vulkan/vulkan.hpp>

namespace vke {

    TrackedImage::TrackedImage(const vk::Image image, const vk::ImageSubresourceRange &subresource_range) {
        m_image = image;
        m_subresource_range = subresource_range;

        // have no clue if this will work. if not, use something different then 0 as the current_owner
        m_state = {.current_layout = vk::ImageLayout::eUndefined, .current_access = vk::AccessFlagBits2::eNone, .current_owner = 0};
    }

    TrackedImage::TrackedImage(const vk::Image image, const vk::ImageSubresourceRange &subresource_range, const ImageState &initial_state) {
        m_image = image;
        m_subresource_range = subresource_range;
        m_state = initial_state;
    }

    void TrackedImage::set_layout(const vk::ImageLayout layout) {
        m_state.current_layout = layout;
    }

    void TrackedImage::set_access(const vk::AccessFlags2 access) {
        m_state.current_access = access;
    }

    void TrackedImage::set_owner(const uint32_t owner) {
        m_state.current_owner = owner;
    }

    void TrackedImage::transition(const vk::CommandBuffer& cmd, vk::PipelineStageFlags2 src_stage, vk::PipelineStageFlags2 dst_stage, const vk::ImageLayout new_layout, const vk::AccessFlags2 new_access, const uint32_t new_owner) {
        vk::ImageMemoryBarrier2 barrier{};
        barrier.image = m_image;
        barrier.subresourceRange = m_subresource_range;
        barrier.oldLayout = m_state.current_layout;
        barrier.newLayout = new_layout;
        barrier.srcAccessMask = m_state.current_access;
        barrier.dstAccessMask = new_access;
        barrier.srcQueueFamilyIndex = m_state.current_owner;
        barrier.dstQueueFamilyIndex = new_owner == VK_QUEUE_FAMILY_IGNORED ? m_state.current_owner : new_owner;
        barrier.srcStageMask = src_stage;
        barrier.dstStageMask = dst_stage;

        cmd.pipelineBarrier2(vk::DependencyInfo({}, {}, {}, barrier));

        set_layout(new_layout);
        set_access(new_access);
        set_owner(barrier.dstQueueFamilyIndex);
    }
} // namespace vke
