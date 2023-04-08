#ifndef COFFEE_GRAPHICS_IMAGE
#define COFFEE_GRAPHICS_IMAGE

#include <coffee/graphics/device.hpp>
#include <coffee/graphics/sampler.hpp>
#include <coffee/utils/non_moveable.hpp>

namespace coffee {

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

    struct ImageViewConfiguration {
        VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D;
        VkFormat format = VK_FORMAT_UNDEFINED;
        VkComponentMapping components {};
        VkImageSubresourceRange subresourceRange { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    };

    class ImageImpl : NonMoveable {
    public:
        // Must be used for regular images BUT NOT FOR SWAPCHAIN ONE'S
        ImageImpl(Device& device, const ImageConfiguration& configuration);

        // Must be used ONLY for swapchain images
        ImageImpl(Device& device, VkFormat imageFormat, VkImage imageImpl, uint32_t width, uint32_t height) noexcept;

        ~ImageImpl() noexcept;

        inline const VkImage& image() const noexcept
        {
            return image_;
        }

        const VkImageType imageType = VK_IMAGE_TYPE_2D;
        const VkFormat imageFormat = VK_FORMAT_UNDEFINED;
        const VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;
        const VkExtent3D extent = {};

    private:
        Device& device_;

        const bool swapChainImage_ = false;

        VmaAllocation allocation_ = VK_NULL_HANDLE;
        VkImage image_ = VK_NULL_HANDLE;

        friend class ImageViewImpl;
    };

    using Image = std::shared_ptr<ImageImpl>;

    class ImageViewImpl {
    public:
        ImageViewImpl(const Image& image, const ImageViewConfiguration& configuration);
        ~ImageViewImpl() noexcept;

        inline VkImageView view() const noexcept
        {
            return view_;
        }

        const VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    private:
        Image image_ = nullptr;
        VkImageView view_ = VK_NULL_HANDLE;
    };

    using ImageView = std::shared_ptr<ImageViewImpl>;

} // namespace coffee

#endif