#ifndef COFFEE_GRAPHICS_IMAGE
#define COFFEE_GRAPHICS_IMAGE

#include <coffee/graphics/device.hpp>
#include <coffee/graphics/sampler.hpp>
#include <coffee/utils/non_moveable.hpp>

namespace coffee {

    namespace graphics {

        struct ImageConfiguration {
            VkImageCreateFlags flags = 0;
            VkImageType imageType = VK_IMAGE_TYPE_2D;
            VkFormat format = VK_FORMAT_UNDEFINED;
            VkExtent3D extent { 0U, 0U, 1U };
            uint32_t mipLevels = 1U;
            uint32_t arrayLayers = 1U;
            VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
            VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
            VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            VmaAllocationCreateFlags allocationFlags = 0;
            float priority = 0.5f;
        };

        struct FSImageConfiguration {
            VkImageCreateFlags flags = 0;
            uint32_t mipLevels = 1U;
            float priority = 0.5f;
        };

        struct ImageViewConfiguration {
            VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D;
            VkFormat format = VK_FORMAT_UNDEFINED;
            VkComponentMapping components {};
            VkImageSubresourceRange subresourceRange { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        };

        class Image;
        using ImagePtr = std::shared_ptr<Image>;

        class ImageView;
        using ImageViewPtr = std::shared_ptr<ImageView>;

        class Image : NonMoveable {
        public:
            ~Image() noexcept;

            static ImagePtr create(const DevicePtr& device, const ImageConfiguration& configuration);

            inline const VkImage& image() const noexcept { return image_; }

            const bool swapChainImage = false;
            const VkImageType imageType = VK_IMAGE_TYPE_2D;
            const VkFormat imageFormat = VK_FORMAT_UNDEFINED;
            const VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;
            const VkExtent3D extent = {};
            const uint32_t mipLevels = 1U;
            const uint32_t arrayLayers = 1U;

        private:
            Image(const DevicePtr& device, const ImageConfiguration& configuration);

            // Swapchain-specific constructor
            Image(const DevicePtr& device, VkFormat imageFormat, VkImage imageImpl, uint32_t width, uint32_t height) noexcept;

            DevicePtr device_;

            VmaAllocation allocation_ = VK_NULL_HANDLE;
            VkImage image_ = VK_NULL_HANDLE;

            friend class ImageView;
            friend class SwapChain;
        };

        class ImageView : NonMoveable {
        public:
            ~ImageView() noexcept;

            static ImageViewPtr create(const ImagePtr& image, const ImageViewConfiguration& configuration);

            inline VkImageView view() const noexcept { return view_; }

            const ImagePtr image;
            const VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

        private:
            ImageView(const ImagePtr& image, const ImageViewConfiguration& configuration);

            VkImageView view_ = VK_NULL_HANDLE;
        };

    } // namespace graphics

} // namespace coffee

#endif