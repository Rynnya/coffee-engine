#include <coffee/graphics/image.hpp>

#include <coffee/graphics/exceptions.hpp>
#include <coffee/utils/log.hpp>

#include <algorithm>

namespace coffee {

    namespace graphics {

        Image::Image(const DevicePtr& device, const ImageConfiguration& configuration)
            : swapChainImage { false }
            , imageType { configuration.imageType }
            , imageFormat { configuration.format }
            , sampleCount { VkUtils::getUsableSampleCount(configuration.samples, device->properties()) }
            , extent { configuration.extent.width, configuration.extent.height, std::max(configuration.extent.depth, 1U) }
            , mipLevels { configuration.mipLevels }
            , arrayLayers { configuration.arrayLayers }
            , device_ { device }
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
            VkResult result = vmaCreateImage(device_->allocator(), &imageCreateInfo, &vmaCreateInfo, &image_, &allocation_, nullptr);

            if (result != VK_SUCCESS) {
                COFFEE_ERROR("VMA failed to allocate image, requested extent {{ {}, {}, {} }}, with {} usage flags!", 
                extent.width, extent.height, extent.depth, format::imageUsageFlags(imageCreateInfo.usage));

                throw RegularVulkanException { result };
            }
        }

        Image::Image(const DevicePtr& device, VkFormat imageFormat, VkImage imageImpl, uint32_t width, uint32_t height) noexcept
            : swapChainImage { true }
            , imageType { VK_IMAGE_TYPE_2D }
            , imageFormat { imageFormat }
            , sampleCount { VK_SAMPLE_COUNT_1_BIT }
            , extent { width, height, 1 }
            , mipLevels { 1U }
            , arrayLayers { 1U }
            , device_ { device }
        {
            COFFEE_ASSERT(device != nullptr, "Invalid device provided.");
            COFFEE_ASSERT(imageImpl != nullptr, "Invalid image handle provided.");

            image_ = imageImpl;
        }

        Image::~Image() noexcept
        {
            // Specification states that we must not free swap chain images
            if (!swapChainImage) {
                vmaDestroyImage(device_->allocator(), image_, allocation_);
            }
        }

        ImagePtr Image::create(const DevicePtr& device, const ImageConfiguration& configuration)
        {
            COFFEE_ASSERT(device != nullptr, "Invalid device provided.");

            return std::shared_ptr<Image>(new Image { device, configuration });
        }

        ImageView::ImageView(const ImagePtr& image, const ImageViewConfiguration& configuration)
            : image { image }
            , aspectMask { configuration.subresourceRange.aspectMask }
        {
            COFFEE_ASSERT(image != nullptr, "Invalid image handle provided.");

            VkImageViewCreateInfo createInfo { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
            createInfo.image = image->image();
            createInfo.viewType = configuration.viewType;
            createInfo.format = configuration.format;
            createInfo.components = configuration.components;
            createInfo.subresourceRange = configuration.subresourceRange;
            VkResult result = vkCreateImageView(image->device_->logicalDevice(), &createInfo, nullptr, &view_);

            if (result != VK_SUCCESS) {
                COFFEE_ERROR("Failed to create image view!");

                throw RegularVulkanException { result };
            }
        }

        ImageView::~ImageView() noexcept { vkDestroyImageView(image->device_->logicalDevice(), view_, nullptr); }

        ImageViewPtr ImageView::create(const ImagePtr& image, const ImageViewConfiguration& configuration)
        {
            return std::shared_ptr<ImageView>(new ImageView { image, configuration });
        }

    } // namespace graphics

} // namespace coffee