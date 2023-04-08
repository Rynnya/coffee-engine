#include <coffee/utils/vk_utils.hpp>

#include <coffee/utils/log.hpp>
#include <coffee/utils/math.hpp>

#include <array>
#include <stdexcept>

namespace coffee {

    VkFormat VkUtils::findSupportedFormat(
        VkPhysicalDevice device,
        const std::vector<VkFormat>& candidates,
        VkImageTiling tiling,
        VkFormatFeatureFlags features
    )
    {
        for (VkFormat format : candidates) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(device, format, &props);

            if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
                return format;
            }

            if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
                return format;
            }
        }

        throw std::runtime_error("failed to find supported format!");
    }

    VkFormat VkUtils::findDepthFormat(VkPhysicalDevice device)
    {
        return findSupportedFormat(
            device,
            {
                // Search in reverse order to get maximal compression that available for this GPU
                VK_FORMAT_D32_SFLOAT_S8_UINT, // This one will have 40 bits of data, 32 for depth and 8 for stencil
                VK_FORMAT_D32_SFLOAT,         // This one doesn't have stencil, so it might be a bad idea to
                                              // ever use it
                VK_FORMAT_D16_UNORM           // This variant is always available for both depth and
                                              // depth-stencil images as fallback
            },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        );
    }

    uint32_t VkUtils::findMemoryType(VkPhysicalDevice device, uint32_t typeFilter, VkMemoryPropertyFlags properties)
    {
        VkPhysicalDeviceMemoryProperties memoryProperties {};
        vkGetPhysicalDeviceMemoryProperties(device, &memoryProperties);

        for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }

        throw std::runtime_error("Failed to find suitable memory type!");
    }

    VkSurfaceFormatKHR VkUtils::chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) noexcept
    {
        for (const auto& availableFormat : availableFormats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return availableFormat;
            }
        }

        // Fallback, most likely will be R8G8B8A8Unorm
        return availableFormats[0];
    }

    VkPresentModeKHR VkUtils::choosePresentMode(
        const std::vector<VkPresentModeKHR>& availablePresentModes,
        VkPresentModeKHR preferable
    ) noexcept
    {
        for (const auto& availablePresentMode : availablePresentModes) {
            if (availablePresentMode == preferable) {
                return availablePresentMode;
            }
        }

        // Always available, should be selected by default
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D VkUtils::chooseExtent(const VkExtent2D& extent, const VkSurfaceCapabilitiesKHR& capabilities) noexcept
    {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        }

        VkExtent2D actualExtent = extent;

        actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
        actualExtent.height =
            std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

        return actualExtent;
    }

    std::vector<VkExtensionProperties> VkUtils::getInstanceExtensions()
    {
        uint32_t extensionCount;
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions { extensionCount };
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());

        return availableExtensions;
    }

    std::vector<VkExtensionProperties> VkUtils::getDeviceExtensions(VkPhysicalDevice device)
    {

        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions { extensionCount };
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        return availableExtensions;
    }

    bool VkUtils::isExtensionsAvailable(
        const std::vector<VkExtensionProperties>& availableExtensions,
        const std::vector<const char*>& requiredExtensions
    ) noexcept
    {
        for (const char* extensions : requiredExtensions) {
            auto detectExtension = [&](const VkExtensionProperties& ext) { return std::strcmp(ext.extensionName, extensions) == 0; };

            if (std::find_if(availableExtensions.begin(), availableExtensions.end(), detectExtension) == availableExtensions.end()) {
                return false;
            }
        }

        return true;
    }

    VkUtils::SwapChainSupportDetails VkUtils::querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) noexcept
    {
        SwapChainSupportDetails details;

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

        uint32_t formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
        if (formatCount != 0) {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
        }

        uint32_t presentModeCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
        if (presentModeCount != 0) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
        }

        return details;
    }

    VkUtils::QueueFamilyIndices VkUtils::findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) noexcept
    {
        QueueFamilyIndices indices;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies { queueFamilyCount };
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        for (size_t i = 0; i < queueFamilies.size(); i++) {
            const auto& queueFamily = queueFamilies[i];

            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphicsFamily = static_cast<uint32_t>(i);
            }
            else if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) {
                // TODO: Add compute queue later on
                // Right now only required for skipping through compute queues to find dedicated transfer queue
            }
            // Only use dedicated family for transfer operations if available
            else if (queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT) {
                indices.transferFamily = static_cast<uint32_t>(i);
            }

            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, static_cast<uint32_t>(i), surface, &presentSupport);

            if (presentSupport) {
                indices.presentFamily = static_cast<uint32_t>(i);
            }

            if (indices.isComplete()) {
                break;
            }
        }

        return indices;
    }

    uint32_t VkUtils::getOptimalAmountOfFramebuffers(VkPhysicalDevice device, VkSurfaceKHR surface) noexcept
    {
        VkSurfaceCapabilitiesKHR capabilities {};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &capabilities);

        uint32_t imageCount = capabilities.minImageCount + 1;
        if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
            imageCount = capabilities.maxImageCount;
        }

        return imageCount;
    }

    VkDescriptorType VkUtils::getBufferDescriptorType(VkBufferUsageFlags bufferFlags, bool isDynamic) noexcept
    {
        if (bufferFlags & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) {
            return isDynamic ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        }

        if (bufferFlags & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) {
            return isDynamic ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        }

        if (bufferFlags & VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT) {
            return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
        }

        if (bufferFlags & VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT) {
            return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
        }

        return VK_DESCRIPTOR_TYPE_MAX_ENUM;
    }

    VkDescriptorType VkUtils::getImageDescriptorType(VkImageUsageFlags imageFlags, bool withSampler) noexcept
    {
        if (imageFlags & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT) {
            return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        }

        if (imageFlags & VK_IMAGE_USAGE_STORAGE_BIT) {
            return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        }

        if (imageFlags & VK_IMAGE_USAGE_SAMPLED_BIT) {
            return withSampler ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        }

        return VK_DESCRIPTOR_TYPE_MAX_ENUM;
    }

    VkSampleCountFlagBits VkUtils::getUsableSampleCount(
        VkSampleCountFlagBits sampleCount,
        const VkPhysicalDeviceProperties& properties
    ) noexcept
    {
        VkSampleCountFlags counts = properties.limits.framebufferColorSampleCounts & properties.limits.framebufferDepthSampleCounts;

        if ((counts & VK_SAMPLE_COUNT_64_BIT) && (sampleCount & VK_SAMPLE_COUNT_64_BIT)) {
            return VK_SAMPLE_COUNT_64_BIT;
        }

        if ((counts & VK_SAMPLE_COUNT_32_BIT) && (sampleCount & VK_SAMPLE_COUNT_32_BIT)) {
            return VK_SAMPLE_COUNT_32_BIT;
        }

        if ((counts & VK_SAMPLE_COUNT_16_BIT) && (sampleCount & VK_SAMPLE_COUNT_16_BIT)) {
            return VK_SAMPLE_COUNT_16_BIT;
        }

        if ((counts & VK_SAMPLE_COUNT_8_BIT) && (sampleCount & VK_SAMPLE_COUNT_8_BIT)) {
            return VK_SAMPLE_COUNT_8_BIT;
        }

        if ((counts & VK_SAMPLE_COUNT_4_BIT) && (sampleCount & VK_SAMPLE_COUNT_4_BIT)) {
            return VK_SAMPLE_COUNT_4_BIT;
        }

        if ((counts & VK_SAMPLE_COUNT_2_BIT) && (sampleCount & VK_SAMPLE_COUNT_2_BIT)) {
            return VK_SAMPLE_COUNT_2_BIT;
        }

        return VK_SAMPLE_COUNT_1_BIT;
    }

} // namespace coffee