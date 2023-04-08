#include <coffee/graphics/sampler.hpp>

#include <coffee/utils/log.hpp>

#include <algorithm>

namespace coffee {

    SamplerImpl::SamplerImpl(Device& device, const SamplerConfiguration& configuration) : device_ { device }
    {
        VkSamplerCreateInfo createInfo { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };

        createInfo.flags = 0;
        createInfo.magFilter = configuration.magFilter;
        createInfo.minFilter = configuration.minFilter;
        createInfo.mipmapMode = configuration.mipmapMode;
        createInfo.addressModeU = configuration.addressModeU;
        createInfo.addressModeV = configuration.addressModeV;
        createInfo.addressModeW = configuration.addressModeW;
        createInfo.mipLodBias = configuration.mipLodBias;
        createInfo.anisotropyEnable = configuration.anisotropyEnable ? VK_TRUE : VK_FALSE;
        createInfo.maxAnisotropy = std::clamp(configuration.maxAnisotropy, 1.0f, device_.properties().limits.maxSamplerAnisotropy);
        createInfo.compareEnable = configuration.compareEnable ? VK_TRUE : VK_FALSE;
        createInfo.compareOp = configuration.compareOp;
        createInfo.minLod = configuration.minLod;
        createInfo.maxLod = configuration.maxLod;
        createInfo.borderColor = configuration.borderColor;
        createInfo.unnormalizedCoordinates = VK_FALSE;

        COFFEE_THROW_IF(vkCreateSampler(device_.logicalDevice(), &createInfo, nullptr, &sampler_), "Failed to create sampler!");
    }

    SamplerImpl::~SamplerImpl() noexcept
    {
        vkDestroySampler(device_.logicalDevice(), sampler_, nullptr);
    }

} // namespace coffee