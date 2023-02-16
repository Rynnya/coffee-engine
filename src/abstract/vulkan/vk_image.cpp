#include <coffee/abstract/vulkan/vk_image.hpp>

#include <coffee/utils/log.hpp>

namespace coffee {

    VulkanImage::VulkanImage(VulkanDevice& device, const ImageConfiguration& configuration)
        : AbstractImage { configuration.type, configuration.width, configuration.height, configuration.depth }
        , swapChainImage { false }
        , device_ { device }
    {
        VkFormat format = VkUtils::transformFormat(configuration.format);
        sampleCount = VkUtils::getUsableSampleCount(configuration.samples, device_.getProperties());

        VkImageCreateInfo imageCreateInfo { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        imageCreateInfo.imageType = VkUtils::transformImageType(configuration.type);
        imageCreateInfo.format = format;
        imageCreateInfo.extent.width = getWidth();
        imageCreateInfo.extent.height = getHeight();
        imageCreateInfo.extent.depth = getDepth();
        imageCreateInfo.mipLevels = configuration.mipLevels;
        imageCreateInfo.arrayLayers = configuration.arrayLayers;
        imageCreateInfo.samples = sampleCount;
        imageCreateInfo.tiling = VkUtils::transformImageTiling(configuration.tiling);
        imageCreateInfo.usage = VkUtils::transformImageUsage(configuration.usage);
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageCreateInfo.queueFamilyIndexCount = 0;      // Ignored when sharingMode is set to exclusive
        imageCreateInfo.pQueueFamilyIndices = nullptr;  // Ignored when sharingMode is set to exclusive
        imageCreateInfo.initialLayout = VkUtils::transformResourceStateToLayout(configuration.initialState);

        COFFEE_THROW_IF(
            vkCreateImage(device_.getLogicalDevice(), &imageCreateInfo, nullptr, &image) != VK_SUCCESS, "Failed to create image!");

        VkMemoryRequirements memoryRequirements {};
        vkGetImageMemoryRequirements(device_.getLogicalDevice(), image, &memoryRequirements);

        VkMemoryAllocateInfo allocateInfo { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        allocateInfo.allocationSize = memoryRequirements.size;
        allocateInfo.memoryTypeIndex = VkUtils::findMemoryType(
            device_.getPhysicalDevice(),
            memoryRequirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        COFFEE_THROW_IF(
            vkAllocateMemory(device_.getLogicalDevice(), &allocateInfo, nullptr, &memory) != VK_SUCCESS, "Failed to allocate memory for image!");
        COFFEE_THROW_IF(
            vkBindImageMemory(device_.getLogicalDevice(), image, memory, 0) != VK_SUCCESS, "Failed to bind image to memory!");

        aspectMask = VkUtils::transformImageAspects(configuration.aspects);

        VkImageViewCreateInfo imageViewCreateInfo { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        imageViewCreateInfo.image = image;
        imageViewCreateInfo.viewType = VkUtils::transformImageViewType(configuration.viewType);
        imageViewCreateInfo.format = format;
        imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.subresourceRange.aspectMask = aspectMask;
        imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        imageViewCreateInfo.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imageViewCreateInfo.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

        COFFEE_THROW_IF(
            vkCreateImageView(device_.getLogicalDevice(), &imageViewCreateInfo, nullptr, &imageView) != VK_SUCCESS, 
            "Failed to create image view!");
    }

    VulkanImage::VulkanImage(VulkanDevice& device, uint32_t width, uint32_t height, VkFormat imageFormat, VkImage imageImpl)
        : AbstractImage { ImageType::TwoDimensional, width, height, 1 }
        , swapChainImage { true }
        , device_ { device }
    {
        image = imageImpl;
        sampleCount = VK_SAMPLE_COUNT_1_BIT;
        aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

        VkImageViewCreateInfo imageViewCreateInfo { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        imageViewCreateInfo.image = image;
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.format = imageFormat;
        imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.subresourceRange.aspectMask = aspectMask;
        imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        imageViewCreateInfo.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imageViewCreateInfo.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

        COFFEE_THROW_IF(
            vkCreateImageView(device_.getLogicalDevice(), &imageViewCreateInfo, nullptr, &imageView) != VK_SUCCESS,
            "Failed to create image view!");
    }

    VulkanImage::~VulkanImage() noexcept {
        vkDestroyImageView(device_.getLogicalDevice(), imageView, nullptr);

        // Specification states that we must not free swap chain images
        if (!swapChainImage) {
            vkFreeMemory(device_.getLogicalDevice(), memory, nullptr);
            vkDestroyImage(device_.getLogicalDevice(), image, nullptr);
        }
    }

}