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
    ) {
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

    VkFormat VkUtils::findDepthFormat(VkPhysicalDevice device) {
        return findSupportedFormat(
            device,
            { 
                VK_FORMAT_D32_SFLOAT_S8_UINT,
                VK_FORMAT_D32_SFLOAT,
                VK_FORMAT_D24_UNORM_S8_UINT,
                VK_FORMAT_D16_UNORM_S8_UINT,
                VK_FORMAT_D16_UNORM
            },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
    }

    uint32_t VkUtils::findMemoryType(VkPhysicalDevice device, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memoryProperties {};
        vkGetPhysicalDeviceMemoryProperties(device, &memoryProperties);

        for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }

        throw std::runtime_error("Failed to find suitable memory type!");
    }

    VkSurfaceFormatKHR VkUtils::chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) noexcept {
        for (const auto& availableFormat : availableFormats) {
            if (
                availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
                availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
            ) {
                return availableFormat;
            }
        }

        // Fallback
        return availableFormats[0];
    }

    VkPresentModeKHR VkUtils::choosePresentMode(
        const std::vector<VkPresentModeKHR>& availablePresentModes,
        const std::optional<VkPresentModeKHR>& preferable
    ) noexcept {
        if (preferable.has_value()) {
            for (const auto& availablePresentMode : availablePresentModes) {
                if (availablePresentMode == preferable.value()) {
                    return availablePresentMode;
                }
            }
        }

        // Always available, should be selected by default
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D VkUtils::chooseExtent(const VkExtent2D& extent, const VkSurfaceCapabilitiesKHR& capabilities) noexcept {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        }

        VkExtent2D actualExtent = extent;

        actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
        actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

        return actualExtent;
    }

    VkUtils::SwapChainSupportDetails VkUtils::querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) noexcept {
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

    VkUtils::QueueFamilyIndices VkUtils::findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) noexcept {
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

            VkBool32 presentSupport = false;
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

    uint32_t VkUtils::getOptionalAmountOfFramebuffers(VkPhysicalDevice device, VkSurfaceKHR surface) noexcept {
        VkSurfaceCapabilitiesKHR capabilities {};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &capabilities);

        uint32_t imageCount = capabilities.minImageCount + 1;
        if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
            imageCount = capabilities.maxImageCount;
        }

        return imageCount;
    }

    VkDescriptorType VkUtils::getBufferDescriptorType(VkBufferUsageFlags bufferFlags, bool isDynamic) noexcept {
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

    VkDescriptorType VkUtils::getImageDescriptorType(VkImageUsageFlags imageFlags, bool withSampler) noexcept {
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

    VkSampleCountFlagBits VkUtils::getUsableSampleCount(uint32_t sampleCount, const VkPhysicalDeviceProperties& properties) noexcept {
        size_t powerOf2SampleCount = Math::roundToPowerOf2(sampleCount);
        VkSampleCountFlags counts = properties.limits.framebufferColorSampleCounts & properties.limits.framebufferDepthSampleCounts;

        if (counts & VK_SAMPLE_COUNT_64_BIT && powerOf2SampleCount >= 64) {
            return VK_SAMPLE_COUNT_64_BIT;
        }

        if (counts & VK_SAMPLE_COUNT_32_BIT && powerOf2SampleCount >= 32) {
            return VK_SAMPLE_COUNT_32_BIT;
        }

        if (counts & VK_SAMPLE_COUNT_16_BIT && powerOf2SampleCount >= 16) {
            return VK_SAMPLE_COUNT_16_BIT;
        }

        if (counts & VK_SAMPLE_COUNT_8_BIT && powerOf2SampleCount >= 8) {
            return VK_SAMPLE_COUNT_8_BIT;
        }

        if (counts & VK_SAMPLE_COUNT_4_BIT && powerOf2SampleCount >= 4) {
            return VK_SAMPLE_COUNT_4_BIT;
        }

        if (counts & VK_SAMPLE_COUNT_2_BIT && powerOf2SampleCount >= 2) {
            return VK_SAMPLE_COUNT_2_BIT;
        }

        return VK_SAMPLE_COUNT_1_BIT;
    }

    VkShaderStageFlagBits VkUtils::transformShaderStage(ShaderStage stage) noexcept {
        switch (stage) {
            case ShaderStage::Vertex:
                return VK_SHADER_STAGE_VERTEX_BIT;
            case ShaderStage::Geometry:
                return VK_SHADER_STAGE_GEOMETRY_BIT;
            case ShaderStage::TessellationControl:
                return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
            case ShaderStage::TessellationEvaluation:
                return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
            case ShaderStage::Fragment:
                return VK_SHADER_STAGE_FRAGMENT_BIT;
            case ShaderStage::Compute:
                return VK_SHADER_STAGE_COMPUTE_BIT;
            case ShaderStage::All:
                return VK_SHADER_STAGE_ALL_GRAPHICS;
            default:
                COFFEE_ASSERT(false, "Invalid ShaderStage provided. This should not happen.");
        }

        return VK_SHADER_STAGE_ALL_GRAPHICS;
    }


    VkShaderStageFlags VkUtils::transformShaderStages(ShaderStage stages) noexcept {
        VkShaderStageFlags flagsImpl = 0;

        if ((stages & ShaderStage::All) == ShaderStage::All) {
            return VK_SHADER_STAGE_ALL_GRAPHICS;
        }

        constexpr std::array<VkShaderStageFlags, 6> bitset = {
            VK_SHADER_STAGE_VERTEX_BIT,
            VK_SHADER_STAGE_GEOMETRY_BIT,
            VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
            VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            VK_SHADER_STAGE_COMPUTE_BIT
        };

        for (uint32_t i = 0; i < bitset.size(); i++) {
            if ((stages & static_cast<ShaderStage>(1 << i)) == static_cast<ShaderStage>(1 << i)) {
                flagsImpl |= bitset[i];
            }
        }

        return flagsImpl;
    }

    VkBufferUsageFlags VkUtils::transformBufferFlags(BufferUsage flags) noexcept {
        VkBufferUsageFlags flagsImpl = 0;

        constexpr std::array<VkMemoryPropertyFlags, 7> bitset = {
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT
        };

        for (uint32_t i = 0; i < bitset.size(); i++) {
            if ((flags & static_cast<BufferUsage>(1 << i)) == static_cast<BufferUsage>(1 << i)) {
                flagsImpl |= bitset[i];
            }
        }

        return flagsImpl;
    }

    VkMemoryPropertyFlags VkUtils::transformMemoryFlags(MemoryProperty flags) noexcept {
        VkMemoryPropertyFlags flagsImpl = 0;

        constexpr std::array<VkMemoryPropertyFlags, 3> bitset = {
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        };

        for (uint32_t i = 0; i < bitset.size(); i++) {
            if ((flags & static_cast<MemoryProperty>(1 << i)) == static_cast<MemoryProperty>(1 << i)) {
                flagsImpl |= bitset[i];
            }
        }

        return flagsImpl;
    }

    VkImageType VkUtils::transformImageType(ImageType type) noexcept {
        switch (type) {
            case ImageType::OneDimensional:
                return VK_IMAGE_TYPE_1D;
            case ImageType::TwoDimensional:
                return VK_IMAGE_TYPE_2D;
            case ImageType::ThreeDimensional:
                return VK_IMAGE_TYPE_3D;
            default:
                COFFEE_ASSERT(false, "Invalid ImageType provided. This should not happen.");
        }

        return VK_IMAGE_TYPE_2D;
    }


    VkImageViewType VkUtils::transformImageViewType(ImageViewType type) noexcept {
        switch (type) {
            case ImageViewType::OneDimensional:
                return VK_IMAGE_VIEW_TYPE_1D;
            case ImageViewType::TwoDimensional:
                return VK_IMAGE_VIEW_TYPE_2D;
            case ImageViewType::ThreeDimensional:
                return VK_IMAGE_VIEW_TYPE_3D;
            case ImageViewType::Cube:
                return VK_IMAGE_VIEW_TYPE_CUBE;
            case ImageViewType::OneDimensionalArray:
                return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
            case ImageViewType::TwoDimensionalArray:
                return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            default:
                COFFEE_ASSERT(false, "Invalid ImageViewType provided. This should not happen.");
        }

        return VK_IMAGE_VIEW_TYPE_2D;
    }

    VkImageUsageFlags VkUtils::transformImageUsage(ImageUsage flags) noexcept {
        VkImageUsageFlags flagsImpl = 0;

        constexpr std::array<VkImageUsageFlags, 8> bitset = {
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_IMAGE_USAGE_STORAGE_BIT,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
            VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT
        };

        for (uint32_t i = 0; i < bitset.size(); i++) {
            if ((flags & static_cast<ImageUsage>(1 << i)) == static_cast<ImageUsage>(1 << i)) {
                flagsImpl |= bitset[i];
            }
        }

        return flagsImpl;
    }

    VkImageAspectFlags VkUtils::transformImageAspects(ImageAspect flags) noexcept {
        VkImageAspectFlags flagsImpl = 0;

        constexpr std::array<VkImageAspectFlags, 4> bitset = {
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_ASPECT_DEPTH_BIT,
            VK_IMAGE_ASPECT_STENCIL_BIT,
            VK_IMAGE_ASPECT_METADATA_BIT
        };

        for (uint32_t i = 0; i < bitset.size(); i++) {
            if ((flags & static_cast<ImageAspect>(1 << i)) == static_cast<ImageAspect>(1 << i)) {
                flagsImpl |= bitset[i];
            }
        }

        return flagsImpl;
    }

    VkImageTiling VkUtils::transformImageTiling(ImageTiling tiling) noexcept {
        switch (tiling) {
            case ImageTiling::Optimal:
                return VK_IMAGE_TILING_OPTIMAL;
            case ImageTiling::Linear:
                return VK_IMAGE_TILING_LINEAR;
            default:
                COFFEE_ASSERT(false, "Invalid ImageTiling provided. This should not happen.");
        }

        return VK_IMAGE_TILING_OPTIMAL;
    }

    VkFilter VkUtils::transformSamplerFilter(SamplerFilter filter) noexcept {
        switch (filter) {
            case SamplerFilter::Nearest:
                return VK_FILTER_NEAREST;
            case SamplerFilter::Linear:
                return VK_FILTER_LINEAR;
            default:
                COFFEE_ASSERT(false, "Invalid VkFilter provided. This should not happen.");
        }

        return VK_FILTER_NEAREST;
    }

    VkSamplerMipmapMode VkUtils::transformSamplerMipmap(SamplerFilter filter) noexcept {
        switch (filter) {
            case SamplerFilter::Nearest:
                return VK_SAMPLER_MIPMAP_MODE_NEAREST;
            case SamplerFilter::Linear:
                return VK_SAMPLER_MIPMAP_MODE_LINEAR;
            default:
                COFFEE_ASSERT(false, "Invalid VkFilter provided. This should not happen.");
        }

        return VK_SAMPLER_MIPMAP_MODE_NEAREST;
    }

    VkFormat VkUtils::transformFormat(Format format) noexcept {
        switch (format) {
            case Format::Undefined:
                return VK_FORMAT_UNDEFINED;
            case Format::R8UNorm:
                return VK_FORMAT_R8_UNORM;
            case Format::R8SNorm:
                return VK_FORMAT_R8_SNORM;
            case Format::R8UScaled:
                return VK_FORMAT_R8_USCALED;
            case Format::R8SScaled:
                return VK_FORMAT_R8_SSCALED;
            case Format::R8UInt:
                return VK_FORMAT_R8_UINT;
            case Format::R8SInt:
                return VK_FORMAT_R8_SINT;
            case Format::R8SRGB:
                return VK_FORMAT_R8_SRGB;
            case Format::R8G8UNorm:
                return VK_FORMAT_R8G8_UNORM;
            case Format::R8G8SNorm:
                return VK_FORMAT_R8G8_SNORM;
            case Format::R8G8UScaled:
                return VK_FORMAT_R8G8_USCALED;
            case Format::R8G8SScaled:
                return VK_FORMAT_R8G8_SSCALED;
            case Format::R8G8UInt:
                return VK_FORMAT_R8G8_UINT;
            case Format::R8G8SInt:
                return VK_FORMAT_R8G8_SINT;
            case Format::R8G8SRGB:
                return VK_FORMAT_R8G8_SRGB;
            case Format::R8G8B8UNorm:
                return VK_FORMAT_R8G8B8_UNORM;
            case Format::R8G8B8SNorm:
                return VK_FORMAT_R8G8B8_SNORM;
            case Format::R8G8B8UScaled:
                return VK_FORMAT_R8G8B8_USCALED;
            case Format::R8G8B8SScaled:
                return VK_FORMAT_R8G8B8_SSCALED;
            case Format::R8G8B8UInt:
                return VK_FORMAT_R8G8B8_UINT;
            case Format::R8G8B8SInt:
                return VK_FORMAT_R8G8B8_SINT;
            case Format::R8G8B8SRGB:
                return VK_FORMAT_R8G8B8_SRGB;
            case Format::B8G8R8UNorm:
                return VK_FORMAT_B8G8R8_UNORM;
            case Format::B8G8R8SNorm:
                return VK_FORMAT_B8G8R8_SNORM;
            case Format::B8G8R8UScaled:
                return VK_FORMAT_B8G8R8_USCALED;
            case Format::B8G8R8SScaled:
                return VK_FORMAT_B8G8R8_SSCALED;
            case Format::B8G8R8UInt:
                return VK_FORMAT_B8G8R8_UINT;
            case Format::B8G8R8SInt:
                return VK_FORMAT_B8G8R8_SINT;
            case Format::B8G8R8SRGB:
                return VK_FORMAT_B8G8R8_SRGB;
            case Format::R8G8B8A8UNorm:
                return VK_FORMAT_R8G8B8A8_UNORM;
            case Format::R8G8B8A8SNorm:
                return VK_FORMAT_R8G8B8A8_SNORM;
            case Format::R8G8B8A8UScaled:
                return VK_FORMAT_R8G8B8A8_USCALED;
            case Format::R8G8B8A8SScaled:
                return VK_FORMAT_R8G8B8A8_SSCALED;
            case Format::R8G8B8A8UInt:
                return VK_FORMAT_R8G8B8A8_UINT;
            case Format::R8G8B8A8SInt:
                return VK_FORMAT_R8G8B8A8_SINT;
            case Format::R8G8B8A8SRGB:
                return VK_FORMAT_R8G8B8A8_SRGB;
            case Format::B8G8R8A8UNorm:
                return VK_FORMAT_B8G8R8A8_UNORM;
            case Format::B8G8R8A8SNorm:
                return VK_FORMAT_B8G8R8A8_SNORM;
            case Format::B8G8R8A8UScaled:
                return VK_FORMAT_B8G8R8A8_USCALED;
            case Format::B8G8R8A8SScaled:
                return VK_FORMAT_B8G8R8A8_SSCALED;
            case Format::B8G8R8A8UInt:
                return VK_FORMAT_B8G8R8A8_UINT;
            case Format::B8G8R8A8SInt:
                return VK_FORMAT_B8G8R8A8_SINT;
            case Format::B8G8R8A8SRGB:
                return VK_FORMAT_B8G8R8A8_SRGB;
            case Format::R16UNorm:
                return VK_FORMAT_R16_UNORM;
            case Format::R16SNorm:
                return VK_FORMAT_R16_SNORM;
            case Format::R16UScaled:
                return VK_FORMAT_R16_USCALED;
            case Format::R16SScaled:
                return VK_FORMAT_R16_SSCALED;
            case Format::R16UInt:
                return VK_FORMAT_R16_UINT;
            case Format::R16SInt:
                return VK_FORMAT_R16_SINT;
            case Format::R16SFloat:
                return VK_FORMAT_R16_SFLOAT;
            case Format::R16G16UNorm:
                return VK_FORMAT_R16G16_UNORM;
            case Format::R16G16SNorm:
                return VK_FORMAT_R16G16_SNORM;
            case Format::R16G16UScaled:
                return VK_FORMAT_R16G16_USCALED;
            case Format::R16G16SScaled:
                return VK_FORMAT_R16G16_SSCALED;
            case Format::R16G16UInt:
                return VK_FORMAT_R16G16_UINT;
            case Format::R16G16SInt:
                return VK_FORMAT_R16G16_SINT;
            case Format::R16G16SFloat:
                return VK_FORMAT_R16G16_SFLOAT;
            case Format::R16G16B16UNorm:
                return VK_FORMAT_R16G16B16_UNORM;
            case Format::R16G16B16SNorm:
                return VK_FORMAT_R16G16B16_SNORM;
            case Format::R16G16B16UScaled:
                return VK_FORMAT_R16G16B16_USCALED;
            case Format::R16G16B16SScaled:
                return VK_FORMAT_R16G16B16_SSCALED;
            case Format::R16G16B16UInt:
                return VK_FORMAT_R16G16B16_UINT;
            case Format::R16G16B16SInt:
                return VK_FORMAT_R16G16B16_SINT;
            case Format::R16G16B16SFloat:
                return VK_FORMAT_R16G16B16_SFLOAT;
            case Format::R16G16B16A16UNorm:
                return VK_FORMAT_R16G16B16A16_UNORM;
            case Format::R16G16B16A16SNorm:
                return VK_FORMAT_R16G16B16A16_SNORM;
            case Format::R16G16B16A16UScaled:
                return VK_FORMAT_R16G16B16A16_USCALED;
            case Format::R16G16B16A16SScaled:
                return VK_FORMAT_R16G16B16A16_SSCALED;
            case Format::R16G16B16A16UInt:
                return VK_FORMAT_R16G16B16A16_UINT;
            case Format::R16G16B16A16SInt:
                return VK_FORMAT_R16G16B16A16_SINT;
            case Format::R16G16B16A16SFloat:
                return VK_FORMAT_R16G16B16A16_SFLOAT;
            case Format::R32UInt:
                return VK_FORMAT_R32_UINT;
            case Format::R32SInt:
                return VK_FORMAT_R32_SINT;
            case Format::R32SFloat:
                return VK_FORMAT_R32_SFLOAT;
            case Format::R32G32UInt:
                return VK_FORMAT_R32G32_UINT;
            case Format::R32G32SInt:
                return VK_FORMAT_R32G32_SINT;
            case Format::R32G32SFloat:
                return VK_FORMAT_R32G32_SFLOAT;
            case Format::R32G32B32UInt:
                return VK_FORMAT_R32G32B32_UINT;
            case Format::R32G32B32SInt:
                return VK_FORMAT_R32G32B32_SINT;
            case Format::R32G32B32SFloat:
                return VK_FORMAT_R32G32B32_SFLOAT;
            case Format::R32G32B32A32UInt:
                return VK_FORMAT_R32G32B32A32_UINT;
            case Format::R32G32B32A32SInt:
                return VK_FORMAT_R32G32B32A32_SINT;
            case Format::R32G32B32A32SFloat:
                return VK_FORMAT_R32G32B32A32_SFLOAT;
            case Format::R64UInt:
                return VK_FORMAT_R64_UINT;
            case Format::R64SInt:
                return VK_FORMAT_R64_SINT;
            case Format::R64SFloat:
                return VK_FORMAT_R64_SFLOAT;
            case Format::R64G64UInt:
                return VK_FORMAT_R64G64_UINT;
            case Format::R64G64SInt:
                return VK_FORMAT_R64G64_SINT;
            case Format::R64G64SFloat:
                return VK_FORMAT_R64G64_SFLOAT;
            case Format::R64G64B64UInt:
                return VK_FORMAT_R64G64B64_UINT;
            case Format::R64G64B64SInt:
                return VK_FORMAT_R64G64B64_SINT;
            case Format::R64G64B64SFloat:
                return VK_FORMAT_R64G64B64_SFLOAT;
            case Format::R64G64B64A64UInt:
                return VK_FORMAT_R64G64B64A64_UINT;
            case Format::R64G64B64A64SInt:
                return VK_FORMAT_R64G64B64A64_SINT;
            case Format::R64G64B64A64SFloat:
                return VK_FORMAT_R64G64B64A64_SFLOAT;
            case Format::S8UInt:
                return VK_FORMAT_S8_UINT;
            case Format::D16UNorm:
                return VK_FORMAT_D16_UNORM;
            case Format::D16UNormS8UInt:
                return VK_FORMAT_D16_UNORM_S8_UINT;
            case Format::D24UNormS8UInt:
                return VK_FORMAT_D24_UNORM_S8_UINT;
            case Format::D32SFloat:
                return VK_FORMAT_D32_SFLOAT;
            case Format::D32SFloatS8UInt:
                return VK_FORMAT_D32_SFLOAT_S8_UINT;
            case Format::R11G11B10UFloat:
                return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
            default:
                COFFEE_ASSERT(false, "Invalid Format provided. This should not happen.");
        }

        return VK_FORMAT_UNDEFINED;
    }

    Format VkUtils::transformFormat(VkFormat format) noexcept {
        switch (format) {
            case VK_FORMAT_UNDEFINED:
                return Format::Undefined;
            case VK_FORMAT_R8_UNORM:
                return Format::R8UNorm;
            case VK_FORMAT_R8_SNORM:
                return Format::R8SNorm;
            case VK_FORMAT_R8_USCALED:
                return Format::R8UScaled;
            case VK_FORMAT_R8_SSCALED:
                return Format::R8SScaled;
            case VK_FORMAT_R8_UINT:
                return Format::R8UInt;
            case VK_FORMAT_R8_SINT:
                return Format::R8SInt;
            case VK_FORMAT_R8_SRGB:
                return Format::R8SRGB;
            case VK_FORMAT_R8G8_UNORM:
                return Format::R8G8UNorm;
            case VK_FORMAT_R8G8_SNORM:
                return Format::R8G8SNorm;
            case VK_FORMAT_R8G8_USCALED:
                return Format::R8G8UScaled;
            case VK_FORMAT_R8G8_SSCALED:
                return Format::R8G8SScaled;
            case VK_FORMAT_R8G8_UINT:
                return Format::R8G8UInt;
            case VK_FORMAT_R8G8_SINT:
                return Format::R8G8SInt;
            case VK_FORMAT_R8G8_SRGB:
                return Format::R8G8SRGB;
            case VK_FORMAT_R8G8B8_UNORM:
                return Format::R8G8B8UNorm;
            case VK_FORMAT_R8G8B8_SNORM:
                return Format::R8G8B8SNorm;
            case VK_FORMAT_R8G8B8_USCALED:
                return Format::R8G8B8UScaled;
            case VK_FORMAT_R8G8B8_SSCALED:
                return Format::R8G8B8SScaled;
            case VK_FORMAT_R8G8B8_UINT:
                return Format::R8G8B8UInt;
            case VK_FORMAT_R8G8B8_SINT:
                return Format::R8G8B8SInt;
            case VK_FORMAT_R8G8B8_SRGB:
                return Format::R8G8B8SRGB;
            case VK_FORMAT_B8G8R8_UNORM:
                return Format::B8G8R8UNorm;
            case VK_FORMAT_B8G8R8_SNORM:
                return Format::B8G8R8SNorm;
            case VK_FORMAT_B8G8R8_USCALED:
                return Format::B8G8R8UScaled;
            case VK_FORMAT_B8G8R8_SSCALED:
                return Format::B8G8R8SScaled;
            case VK_FORMAT_B8G8R8_UINT:
                return Format::B8G8R8UInt;
            case VK_FORMAT_B8G8R8_SINT:
                return Format::B8G8R8SInt;
            case VK_FORMAT_B8G8R8_SRGB:
                return Format::B8G8R8SRGB;
            case VK_FORMAT_R8G8B8A8_UNORM:
                return Format::R8G8B8A8UNorm;
            case VK_FORMAT_R8G8B8A8_SNORM:
                return Format::R8G8B8A8SNorm;
            case VK_FORMAT_R8G8B8A8_USCALED:
                return Format::R8G8B8A8UScaled;
            case VK_FORMAT_R8G8B8A8_SSCALED:
                return Format::R8G8B8A8SScaled;
            case VK_FORMAT_R8G8B8A8_UINT:
                return Format::R8G8B8A8UInt;
            case VK_FORMAT_R8G8B8A8_SINT:
                return Format::R8G8B8A8SInt;
            case VK_FORMAT_R8G8B8A8_SRGB:
                return Format::R8G8B8A8SRGB;
            case VK_FORMAT_B8G8R8A8_UNORM:
                return Format::B8G8R8A8UNorm;
            case VK_FORMAT_B8G8R8A8_SNORM:
                return Format::B8G8R8A8SNorm;
            case VK_FORMAT_B8G8R8A8_USCALED:
                return Format::B8G8R8A8UScaled;
            case VK_FORMAT_B8G8R8A8_SSCALED:
                return Format::B8G8R8A8SScaled;
            case VK_FORMAT_B8G8R8A8_UINT:
                return Format::B8G8R8A8UInt;
            case VK_FORMAT_B8G8R8A8_SINT:
                return Format::B8G8R8A8SInt;
            case VK_FORMAT_B8G8R8A8_SRGB:
                return Format::B8G8R8A8SRGB;
            case VK_FORMAT_R16_UNORM:
                return Format::R16UNorm;
            case VK_FORMAT_R16_SNORM:
                return Format::R16SNorm;
            case VK_FORMAT_R16_USCALED:
                return Format::R16UScaled;
            case VK_FORMAT_R16_SSCALED:
                return Format::R16SScaled;
            case VK_FORMAT_R16_UINT:
                return Format::R16UInt;
            case VK_FORMAT_R16_SINT:
                return Format::R16SInt;
            case VK_FORMAT_R16_SFLOAT:
                return Format::R16SFloat;
            case VK_FORMAT_R16G16_UNORM:
                return Format::R16G16UNorm;
            case VK_FORMAT_R16G16_SNORM:
                return Format::R16G16SNorm;
            case VK_FORMAT_R16G16_USCALED:
                return Format::R16G16UScaled;
            case VK_FORMAT_R16G16_SSCALED:
                return Format::R16G16SScaled;
            case VK_FORMAT_R16G16_UINT:
                return Format::R16G16UInt;
            case VK_FORMAT_R16G16_SINT:
                return Format::R16G16SInt;
            case VK_FORMAT_R16G16_SFLOAT:
                return Format::R16G16SFloat;
            case VK_FORMAT_R16G16B16_UNORM:
                return Format::R16G16B16UNorm;
            case VK_FORMAT_R16G16B16_SNORM:
                return Format::R16G16B16SNorm;
            case VK_FORMAT_R16G16B16_USCALED:
                return Format::R16G16B16UScaled;
            case VK_FORMAT_R16G16B16_SSCALED:
                return Format::R16G16B16SScaled;
            case VK_FORMAT_R16G16B16_UINT:
                return Format::R16G16B16UInt;
            case VK_FORMAT_R16G16B16_SINT:
                return Format::R16G16B16SInt;
            case VK_FORMAT_R16G16B16_SFLOAT:
                return Format::R16G16B16SFloat;
            case VK_FORMAT_R16G16B16A16_UNORM:
                return Format::R16G16B16A16UNorm;
            case VK_FORMAT_R16G16B16A16_SNORM:
                return Format::R16G16B16A16SNorm;
            case VK_FORMAT_R16G16B16A16_USCALED:
                return Format::R16G16B16A16UScaled;
            case VK_FORMAT_R16G16B16A16_SSCALED:
                return Format::R16G16B16A16SScaled;
            case VK_FORMAT_R16G16B16A16_UINT:
                return Format::R16G16B16A16UInt;
            case VK_FORMAT_R16G16B16A16_SINT:
                return Format::R16G16B16A16SInt;
            case VK_FORMAT_R16G16B16A16_SFLOAT:
                return Format::R16G16B16A16SFloat;
            case VK_FORMAT_R32_UINT:
                return Format::R32UInt;
            case VK_FORMAT_R32_SINT:
                return Format::R32SInt;
            case VK_FORMAT_R32_SFLOAT:
                return Format::R32SFloat;
            case VK_FORMAT_R32G32_UINT:
                return Format::R32G32UInt;
            case VK_FORMAT_R32G32_SINT:
                return Format::R32G32SInt;
            case VK_FORMAT_R32G32_SFLOAT:
                return Format::R32G32SFloat;
            case VK_FORMAT_R32G32B32_UINT:
                return Format::R32G32B32UInt;
            case VK_FORMAT_R32G32B32_SINT:
                return Format::R32G32B32SInt;
            case VK_FORMAT_R32G32B32_SFLOAT:
                return Format::R32G32B32SFloat;
            case VK_FORMAT_R32G32B32A32_UINT:
                return Format::R32G32B32A32UInt;
            case VK_FORMAT_R32G32B32A32_SINT:
                return Format::R32G32B32A32SInt;
            case VK_FORMAT_R32G32B32A32_SFLOAT:
                return Format::R32G32B32A32SFloat;
            case VK_FORMAT_R64_UINT:
                return Format::R64UInt;
            case VK_FORMAT_R64_SINT:
                return Format::R64SInt;
            case VK_FORMAT_R64_SFLOAT:
                return Format::R64SFloat;
            case VK_FORMAT_R64G64_UINT:
                return Format::R64G64UInt;
            case VK_FORMAT_R64G64_SINT:
                return Format::R64G64SInt;
            case VK_FORMAT_R64G64_SFLOAT:
                return Format::R64G64SFloat;
            case VK_FORMAT_R64G64B64_UINT:
                return Format::R64G64B64UInt;
            case VK_FORMAT_R64G64B64_SINT:
                return Format::R64G64B64SInt;
            case VK_FORMAT_R64G64B64_SFLOAT:
                return Format::R64G64B64SFloat;
            case VK_FORMAT_R64G64B64A64_UINT:
                return Format::R64G64B64A64UInt;
            case VK_FORMAT_R64G64B64A64_SINT:
                return Format::R64G64B64A64SInt;
            case VK_FORMAT_R64G64B64A64_SFLOAT:
                return Format::R64G64B64A64SFloat;
            case VK_FORMAT_S8_UINT:
                return Format::S8UInt;
            case VK_FORMAT_D16_UNORM:
                return Format::D16UNorm;
            case VK_FORMAT_D16_UNORM_S8_UINT:
                return Format::D16UNormS8UInt;
            case VK_FORMAT_D24_UNORM_S8_UINT:
                return Format::D24UNormS8UInt;
            case VK_FORMAT_D32_SFLOAT:
                return Format::D32SFloat;
            case VK_FORMAT_D32_SFLOAT_S8_UINT:
                return Format::D32SFloatS8UInt;
            case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
                return Format::R11G11B10UFloat;
            default:
                COFFEE_ASSERT(false, "Invalid Format provided. This should not happen.");
        }

        return Format::Undefined;
    }

    VkPrimitiveTopology VkUtils::transformTopology(Topology topology) noexcept {
        switch (topology) {
            case Topology::Point:
                return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
            case Topology::Line:
                return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
            case Topology::Triangle:
                return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            default:
                COFFEE_ASSERT(false, "Invalid Topology provided. This should not happen.");
        }

        return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    }

    VkBlendFactor VkUtils::transformBlendFactor(BlendFactor factor) noexcept {
        switch (factor) {
            case BlendFactor::Zero:
                return VK_BLEND_FACTOR_ZERO;
            case BlendFactor::One:
                return VK_BLEND_FACTOR_ONE;
            case BlendFactor::SrcColor:
                return VK_BLEND_FACTOR_SRC_COLOR;
            case BlendFactor::OneMinusSrcColor:
                return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
            case BlendFactor::DstColor:
                return VK_BLEND_FACTOR_DST_COLOR;
            case BlendFactor::OneMinusDstColor:
                return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
            case BlendFactor::SrcAlpha:
                return VK_BLEND_FACTOR_SRC_ALPHA;
            case BlendFactor::OneMinusSrcAlpha:
                return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            case BlendFactor::DstAlpha:
                return VK_BLEND_FACTOR_DST_ALPHA;
            case BlendFactor::OneMinusDstAlpha:
                return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
            case BlendFactor::ConstantColor:
                return VK_BLEND_FACTOR_CONSTANT_COLOR;
            case BlendFactor::OneMinusConstantColor:
                return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
            case BlendFactor::ConstantAlpha:
                return VK_BLEND_FACTOR_CONSTANT_ALPHA;
            case BlendFactor::OneMinusConstantAlpha:
                return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
            case BlendFactor::SrcAlphaSaturate:
                return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
            case BlendFactor::Src1Color:
                return VK_BLEND_FACTOR_SRC1_COLOR;
            case BlendFactor::OneMinusSrc1Color:
                return VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
            case BlendFactor::Src1Alpha:
                return VK_BLEND_FACTOR_SRC1_ALPHA;
            case BlendFactor::OneMinusSrc1Alpha:
                return VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
            default:
                COFFEE_ASSERT(false, "Invalid BlendFactor provided. This should not happen.");
        }

        return VK_BLEND_FACTOR_ZERO;
    }

    VkBlendOp VkUtils::transformBlendOp(BlendOp op) noexcept {
        switch (op) {
            case BlendOp::Add:
                return VK_BLEND_OP_ADD;
            case BlendOp::Subtract:
                return VK_BLEND_OP_SUBTRACT;
            case BlendOp::ReverseSubtract:
                return VK_BLEND_OP_REVERSE_SUBTRACT;
            case BlendOp::Min:
                return VK_BLEND_OP_SUBTRACT;
            case BlendOp::Max:
                return VK_BLEND_OP_SUBTRACT;
            default:
                COFFEE_ASSERT(false, "Invalid BlendOp provided. This should not happen.");
        }

        return VK_BLEND_OP_ADD;
    }

    VkLogicOp VkUtils::transformLogicOp(LogicOp op) noexcept {
        switch (op) {
            case LogicOp::Clear:
                return VK_LOGIC_OP_CLEAR;
            case LogicOp::And:
                return VK_LOGIC_OP_AND;
            case LogicOp::ReverseAnd:
                return VK_LOGIC_OP_AND_REVERSE;
            case LogicOp::Copy:
                return VK_LOGIC_OP_COPY;
            case LogicOp::InvertedAnd:
                return VK_LOGIC_OP_AND_INVERTED;
            case LogicOp::NoOp:
                return VK_LOGIC_OP_NO_OP;
            case LogicOp::XOR:
                return VK_LOGIC_OP_XOR;
            case LogicOp::OR:
                return VK_LOGIC_OP_OR;
            case LogicOp::NOR:
                return VK_LOGIC_OP_NOR;
            case LogicOp::Equivalent:
                return VK_LOGIC_OP_EQUIVALENT;
            case LogicOp::Invert:
                return VK_LOGIC_OP_INVERT;
            case LogicOp::ReverseOR:
                return VK_LOGIC_OP_OR_REVERSE;
            case LogicOp::InvertedCopy:
                return VK_LOGIC_OP_COPY_INVERTED;
            case LogicOp::InvertedOR:
                return VK_LOGIC_OP_OR_INVERTED;
            case LogicOp::NAND:
                return VK_LOGIC_OP_NAND;
            case LogicOp::Set:
                return VK_LOGIC_OP_SET;
            default:
                COFFEE_ASSERT(false, "Invalid LogicOp provided. This should not happen.");
        }

        return VK_LOGIC_OP_CLEAR;
    }


    VkAttachmentLoadOp VkUtils::transformAttachmentLoadOp(AttachmentLoadOp op) noexcept {
        switch (op) {
            case AttachmentLoadOp::Load:
                return VK_ATTACHMENT_LOAD_OP_LOAD;
            case AttachmentLoadOp::Clear:
                return VK_ATTACHMENT_LOAD_OP_CLEAR;
            case AttachmentLoadOp::DontCare:
                return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            default:
                COFFEE_ASSERT(false, "Invalid AttachmentLoadOp provided. This should not happen.");
        }

        return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    }

    VkAttachmentStoreOp VkUtils::transformAttachmentStoreOp(AttachmentStoreOp op) noexcept {
        switch (op) {
            case AttachmentStoreOp::Store:
                return VK_ATTACHMENT_STORE_OP_STORE;
            case AttachmentStoreOp::DontCare:
                return VK_ATTACHMENT_STORE_OP_DONT_CARE;
            default:
                COFFEE_ASSERT(false, "Invalid AttachmentStoreOp provided. This should not happen.");
        }

        return VK_ATTACHMENT_STORE_OP_DONT_CARE;
    }

    VkColorComponentFlags VkUtils::transformColorComponents(ColorComponent components) noexcept {
        VkColorComponentFlags flagsImpl = 0;

        constexpr std::array<VkColorComponentFlags, 4> bitset = {
            VK_COLOR_COMPONENT_R_BIT,
            VK_COLOR_COMPONENT_G_BIT,
            VK_COLOR_COMPONENT_B_BIT,
            VK_COLOR_COMPONENT_A_BIT
        };

        for (uint32_t i = 0; i < bitset.size(); i++) {
            if ((components & static_cast<ColorComponent>(1 << i)) == static_cast<ColorComponent>(1 << i)) {
                flagsImpl |= bitset[i];
            }
        }

        return flagsImpl;
    }

    VkCompareOp VkUtils::transformCompareOp(CompareOp op) noexcept {
        switch (op) {
            case CompareOp::Never:
                return VK_COMPARE_OP_NEVER;
            case CompareOp::Less:
                return VK_COMPARE_OP_LESS;
            case CompareOp::Equal:
                return VK_COMPARE_OP_EQUAL;
            case CompareOp::LessEqual:
                return VK_COMPARE_OP_LESS_OR_EQUAL;
            case CompareOp::Greater:
                return VK_COMPARE_OP_GREATER;
            case CompareOp::NotEqual:
                return VK_COMPARE_OP_NOT_EQUAL;
            case CompareOp::GreaterEqual:
                return VK_COMPARE_OP_GREATER_OR_EQUAL;
            case CompareOp::Always:
                return VK_COMPARE_OP_ALWAYS;
            default:
                COFFEE_ASSERT(false, "Invalid CompareOp provided. This should not happen.");
        }

        return VK_COMPARE_OP_NEVER;
    }

    VkStencilOp VkUtils::transformStencilOp(StencilOp op) noexcept {
        switch (op) {
            case StencilOp::Keep:
                return VK_STENCIL_OP_KEEP;
            case StencilOp::Zero:
                return VK_STENCIL_OP_ZERO;
            case StencilOp::Replace:
                return VK_STENCIL_OP_REPLACE;
            case StencilOp::IncreaseAndClamp:
                return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
            case StencilOp::DecreaseAndClamp:
                return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
            case StencilOp::Invert:
                return VK_STENCIL_OP_INVERT;
            case StencilOp::IncreaseAndWrap:
                return VK_STENCIL_OP_INCREMENT_AND_WRAP;
            case StencilOp::DecreaseAndWrap:
                return VK_STENCIL_OP_DECREMENT_AND_WRAP;
            default:
                COFFEE_ASSERT(false, "Invalid StencilOp provided. This should not happen.");
        }

        return VK_STENCIL_OP_KEEP;
    }

    VkCullModeFlags VkUtils::transformCullMode(CullMode mode) noexcept {
        switch (mode) {
            case CullMode::None:
                return VK_CULL_MODE_NONE;
            case CullMode::Front:
                return VK_CULL_MODE_FRONT_BIT;
            case CullMode::Back:
                return VK_CULL_MODE_BACK_BIT;
            default:
                COFFEE_ASSERT(false, "Invalid CullMode provided. This should not happen.");
        }

        return VK_CULL_MODE_NONE;
    }

    VkFrontFace VkUtils::transformFrontFace(FrontFace face) noexcept {
        switch (face) {
            case FrontFace::Clockwise:
                return VK_FRONT_FACE_CLOCKWISE;
            case FrontFace::CounterClockwise:
                return VK_FRONT_FACE_COUNTER_CLOCKWISE;
            default:
                COFFEE_ASSERT(false, "Invalid FrontFace provided. This should not happen.");
        }

        return VK_FRONT_FACE_CLOCKWISE;
    }

    VkPolygonMode VkUtils::transformPolygonMode(PolygonMode mode) noexcept {
        switch (mode) {
            case PolygonMode::Wireframe:
                return VK_POLYGON_MODE_LINE;
            case PolygonMode::Solid:
                return VK_POLYGON_MODE_FILL;
            default:
                COFFEE_ASSERT(false, "Invalid PolygonMode provided. This should not happen.");
        }

        return VK_POLYGON_MODE_LINE;
    }

    VkVertexInputRate VkUtils::transformInputRate(InputRate inputRate) noexcept {
        switch (inputRate) {
            case InputRate::PerVertex:
                return VK_VERTEX_INPUT_RATE_VERTEX;
            case InputRate::PerInstance:
                return VK_VERTEX_INPUT_RATE_INSTANCE;
            default:
                COFFEE_ASSERT(false, "Invalid InputRate provided. This should not happen.");
        }

        return VK_VERTEX_INPUT_RATE_VERTEX;
    }

    VkSamplerAddressMode VkUtils::transformAddressMode(AddressMode mode) noexcept {
        switch (mode) {
            case AddressMode::Repeat:
                return VK_SAMPLER_ADDRESS_MODE_REPEAT;
            case AddressMode::MirroredRepeat:
                return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
            case AddressMode::ClampToEdge:
                return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            case AddressMode::ClampToBorder:
                return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
            default:
                COFFEE_ASSERT(false, "Invalid AddressMode provided. This should not happen.");
        }

        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }

    VkBorderColor VkUtils::transformBorderColor(BorderColor color) noexcept {
        switch (color) {
            case BorderColor::TransparentBlack:
                return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
            case BorderColor::OpaqueBlack:
                return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
            case BorderColor::OpaqueWhite:
                return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
            default:
                COFFEE_ASSERT(false, "Invalid BorderColor provided. This should not happen.");
        }

        return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    }

    VkDescriptorType VkUtils::transformDescriptorType(DescriptorType type) noexcept {
        switch (type) {
            case DescriptorType::Sampler:
                return VK_DESCRIPTOR_TYPE_SAMPLER;
            case DescriptorType::ImageAndSampler:
                return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            case DescriptorType::SampledImage:
                return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            case DescriptorType::StorageImage:
                return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            case DescriptorType::UniformBuffer:
                return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            case DescriptorType::StorageBuffer:
                return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            case DescriptorType::InputAttachment:
                return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
            default:
                COFFEE_ASSERT(false, "Invalid DescriptorType provided. This should not happen.");
        }

        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    }

    VkImageLayout VkUtils::transformResourceStateToLayout(ResourceState state) noexcept {
        switch (state) {
            case ResourceState::Undefined:
                return VK_IMAGE_LAYOUT_UNDEFINED;
            case ResourceState::RenderTarget:
                return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            case ResourceState::UnorderedAccess:
                return VK_IMAGE_LAYOUT_GENERAL;
            case ResourceState::DepthRead:
                return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            case ResourceState::DepthWrite:
                return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            case ResourceState::ShaderResource:
                return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            case ResourceState::CopyDestination:
                return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            case ResourceState::CopySource:
                return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            case ResourceState::Present:
                return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            default:
                COFFEE_ASSERT(false, "Invalid ResourceState provided. This should not happen.");
        }

        return VK_IMAGE_LAYOUT_UNDEFINED;
    }

    VkAccessFlags VkUtils::transformResourceStateToAccess(ResourceState state) noexcept {
        VkAccessFlags flagsImpl = 0;

        constexpr std::array<VkAccessFlags, 13> bitset = {
            0,
            VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
            VK_ACCESS_UNIFORM_READ_BIT,
            VK_ACCESS_INDEX_READ_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_TRANSFER_READ_BIT,
            VK_ACCESS_MEMORY_READ_BIT
        };

        for (uint32_t i = 0; i < bitset.size(); i++) {
            if ((state & static_cast<ResourceState>(1 << i)) == static_cast<ResourceState>(1 << i)) {
                flagsImpl |= bitset[i];
            }
        }

        return flagsImpl;
    }

    VkAccessFlags VkUtils::imageLayoutToAccessFlags(VkImageLayout layout) noexcept {
        switch (layout) {
            case VK_IMAGE_LAYOUT_GENERAL:
                return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
                return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                return VK_ACCESS_SHADER_READ_BIT;
            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                return VK_ACCESS_TRANSFER_READ_BIT;
            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                return VK_ACCESS_TRANSFER_WRITE_BIT;
            case VK_IMAGE_LAYOUT_PREINITIALIZED:
                return VK_ACCESS_HOST_WRITE_BIT;
            case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL:
                return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
            case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL:
                return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
            case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
                return VK_ACCESS_MEMORY_READ_BIT;
            case VK_IMAGE_LAYOUT_UNDEFINED:
            default:
                return 0;
        }

        return 0;
    }

    VkPipelineStageFlags VkUtils::accessFlagsToPipelineStages(VkAccessFlags flags) noexcept {
        VkPipelineStageFlags flagsImpl = 0;

        while (flags != 0) {
            VkAccessFlagBits accessFlag = static_cast<VkAccessFlagBits>(Math::getLowestBit(static_cast<size_t>(flags)));
            flags = static_cast<VkAccessFlags>(Math::excludeLowestBit(static_cast<size_t>(flags)));

            switch (accessFlag) {
                case VK_ACCESS_INDIRECT_COMMAND_READ_BIT:
                    flagsImpl |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
                    break;
                case VK_ACCESS_INDEX_READ_BIT:
                    flagsImpl |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
                    break;
                case VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT:
                    flagsImpl |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
                    break;
                case VK_ACCESS_UNIFORM_READ_BIT:
                    flagsImpl |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                    break;
                case VK_ACCESS_INPUT_ATTACHMENT_READ_BIT:
                    flagsImpl |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                    break;
                case VK_ACCESS_SHADER_READ_BIT:
                    flagsImpl |=  VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                    break;
                case VK_ACCESS_SHADER_WRITE_BIT:
                    flagsImpl |=  VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                    break;
                case VK_ACCESS_COLOR_ATTACHMENT_READ_BIT:
                    flagsImpl |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                    break;
                case VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT:
                    flagsImpl |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                    break;
                case VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT:
                    flagsImpl |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
                    break;
                case VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT:
                    flagsImpl |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
                    break;
                case VK_ACCESS_TRANSFER_READ_BIT:
                    flagsImpl |= VK_PIPELINE_STAGE_TRANSFER_BIT;
                    break;
                case VK_ACCESS_TRANSFER_WRITE_BIT:
                    flagsImpl |= VK_PIPELINE_STAGE_TRANSFER_BIT;
                    break;
                case VK_ACCESS_HOST_READ_BIT:
                    flagsImpl |= VK_PIPELINE_STAGE_HOST_BIT;
                    break;
                case VK_ACCESS_HOST_WRITE_BIT:
                    flagsImpl |= VK_PIPELINE_STAGE_HOST_BIT;
                    break;
                case VK_ACCESS_MEMORY_READ_BIT:
                case VK_ACCESS_MEMORY_WRITE_BIT:
                default:
                    break;
            }

        }

        return flagsImpl;
    }

}