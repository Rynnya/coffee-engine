#include <coffee/abstract/vulkan/vk_sampler.hpp>

#include <coffee/utils/log.hpp>
#include <coffee/utils/math.hpp>
#include <coffee/utils/vk_utils.hpp>

namespace coffee {

    VulkanSampler::VulkanSampler(VulkanDevice& device, const SamplerConfiguration& configuration) : device_ { device } {
        VkSamplerCreateInfo createInfo { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };

        createInfo.magFilter = VkUtils::transformSamplerFilter(configuration.magFilter);
        createInfo.minFilter = VkUtils::transformSamplerFilter(configuration.minFilter);
        createInfo.addressModeU = VkUtils::transformAddressMode(configuration.addressModeU);
        createInfo.addressModeV = VkUtils::transformAddressMode(configuration.addressModeV);
        createInfo.addressModeW = VkUtils::transformAddressMode(configuration.addressModeW);
        createInfo.mipLodBias = configuration.mipLodBias;
        createInfo.anisotropyEnable = configuration.anisotropyEnable ? VK_TRUE : VK_FALSE;
        createInfo.maxAnisotropy = std::min(
            static_cast<float>(Math::roundToPowerOf2(configuration.maxAnisotropy)),
            device_.getProperties().limits.maxSamplerAnisotropy
        );
        createInfo.compareEnable = configuration.compareOp != CompareOp::Never ? VK_TRUE : VK_FALSE;
        createInfo.compareOp = VkUtils::transformCompareOp(configuration.compareOp);
        createInfo.mipmapMode = VkUtils::transformSamplerMipmap(configuration.mipmapMode);
        createInfo.borderColor = VkUtils::transformBorderColor(configuration.borderColor);
        createInfo.minLod = configuration.minLod;
        createInfo.maxLod = configuration.maxLod;
        createInfo.unnormalizedCoordinates = VK_FALSE;

        COFFEE_THROW_IF(vkCreateSampler(device_.getLogicalDevice(), &createInfo, nullptr, &sampler), "Failed to create sampler!");
    }

    VulkanSampler::~VulkanSampler() noexcept {
        vkDestroySampler(device_.getLogicalDevice(), sampler, nullptr);
    }

} // namespace coffee