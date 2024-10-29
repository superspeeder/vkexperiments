#pragma once

#include <vulkan/vulkan.hpp>

// This file contains some utility wrappers that assist with tracking the state of various resources, making this like image layout transitions significantly easier
// none of these classes take ownership of any resources

namespace vke {

    struct ImageState {
        vk::ImageLayout  current_layout;
        vk::AccessFlags2 current_access;
        uint32_t         current_owner;

        constexpr friend bool operator==(const ImageState &lhs, const ImageState &rhs) {
            return lhs.current_layout == rhs.current_layout && lhs.current_access == rhs.current_access && lhs.current_owner == rhs.current_owner;
        }

        constexpr friend bool operator!=(const ImageState &lhs, const ImageState &rhs) { return !(lhs == rhs); }
    };

    // TODO: maybe consider how the subresource ranges interact with layout transitions and the like for better safety
    class TrackedImage {
      public:
        TrackedImage(vk::Image image, const vk::ImageSubresourceRange &subresource_range);
        TrackedImage(vk::Image image, const vk::ImageSubresourceRange &subresource_range, const ImageState &initial_state);

        TrackedImage(const TrackedImage &other)     = delete;
        TrackedImage(TrackedImage &&other) noexcept = default;

        TrackedImage &operator=(const TrackedImage &other)     = delete;
        TrackedImage &operator=(TrackedImage &&other) noexcept = default;

        // These 3 functions are not transformative (they are here to write state changes which aren't tracked through actions taken in here, for example switching an image to the
        // undefined layout at the start of a frame)
        void set_layout(vk::ImageLayout layout);
        void set_access(vk::AccessFlags2 access);
        void set_owner(uint32_t owner);

        void transition(const vk::CommandBuffer &cmd, vk::PipelineStageFlags2 src_stage, vk::PipelineStageFlags2 dst_stage, vk::ImageLayout new_layout, vk::AccessFlags2 new_access,
                        uint32_t new_owner = vk::QueueFamilyIgnored);

      private:
        vk::Image                 m_image;
        vk::ImageSubresourceRange m_subresource_range;
        ImageState                m_state;
    };
} // namespace vke
