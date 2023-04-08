#include <coffee/graphics/image.hpp>

#include <coffee/utils/log.hpp>

#include <algorithm>

namespace coffee {

    ImageImpl::ImageImpl(Device& device, const ImageConfiguration& configuration)
        : imageType { configuration.imageType }
        , imageFormat { configuration.format }
        , extent { configuration.extent.width, configuration.extent.height, std::max(configuration.extent.depth, 1U) }
        , sampleCount { VkUtils::getUsableSampleCount(configuration.samples, device.properties()) }
        , device_ { device }
        , swapChainImage_ { false }
    {
        VkImageCreateInfo imageCreateInfo { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        imageCreateInfo.flags = configuration.flags;
        imageCreateInfo.imageType = configuration.imageType;
        imageCreateInfo.format = configuration.format;
        imageCreateInfo.extent = extent;
        imageCreateInfo.mipLevels = configuration.mipLevels;
        imageCreateInfo.arrayLayers = configuration.arrayLayers;
        imageCreateInfo.samples = sampleCount;
        imageCreateInfo.tiling = configuration.tiling;
        imageCreateInfo.usage = configuration.usage;
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageCreateInfo.queueFamilyIndexCount = 0;     // Ignored when sharingMode is set to exclusive
        imageCreateInfo.pQueueFamilyIndices = nullptr; // Ignored when sharingMode is set to exclusive
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo vmaCreateInfo {};
        vmaCreateInfo.flags = configuration.allocationFlags;
        vmaCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
        vmaCreateInfo.priority = std::clamp(configuration.priority, 0.0f, 1.0f);

        COFFEE_THROW_IF(
            vmaCreateImage(device_.allocator(), &imageCreateInfo, &vmaCreateInfo, &image_, &allocation_, nullptr) != VK_SUCCESS, 
            "VMA failed to allocate and bind memory for image!");
    }

    ImageImpl::ImageImpl(Device& device, VkFormat imageFormat, VkImage imageImpl, uint32_t width, uint32_t height) noexcept
        : imageType { VK_IMAGE_TYPE_2D }
        , imageFormat { imageFormat }
        , extent { width, height, 1 }
        , sampleCount { VK_SAMPLE_COUNT_1_BIT }
        , device_ { device }
        , swapChainImage_ { true }
    {
        COFFEE_ASSERT(imageImpl != nullptr, "Invalid image handle provided.");

        image_ = imageImpl;
    }

    ImageImpl::~ImageImpl() noexcept
    {
        // Specification states that we must not free swap chain images
        if (!swapChainImage_) {
            vmaDestroyImage(device_.allocator(), image_, allocation_);
        }
    }

    ImageViewImpl::ImageViewImpl(const Image& image, const ImageViewConfiguration& configuration)
        : aspectMask { configuration.subresourceRange.aspectMask }
        , image_ { image }
    {
        COFFEE_ASSERT(image_ != nullptr, "Invalid image handle provided.");

        VkImageViewCreateInfo createInfo { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        createInfo.image = image_->image();
        createInfo.viewType = configuration.viewType;
        createInfo.format = configuration.format;
        createInfo.components = configuration.components;
        createInfo.subresourceRange = configuration.subresourceRange;

        COFFEE_THROW_IF(
            vkCreateImageView(image_->device_.logicalDevice(), &createInfo, nullptr, &view_) != VK_SUCCESS, 
            "Failed to create image view!");
    }

    ImageViewImpl::~ImageViewImpl() noexcept
    {
        vkDestroyImageView(image_->device_.logicalDevice(), view_, nullptr);
    }

} // namespace coffee