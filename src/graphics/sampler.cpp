#include <coffee/graphics/sampler.hpp>

#include <coffee/graphics/exceptions.hpp>
#include <coffee/utils/log.hpp>

#include <algorithm>

namespace coffee {

    namespace graphics {

        Sampler::Sampler(const DevicePtr& device, const SamplerConfiguration& configuration) : device_ { device }
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
            createInfo.maxAnisotropy = std::clamp(configuration.maxAnisotropy, 1.0f, device_->properties().limits.maxSamplerAnisotropy);
            createInfo.compareEnable = configuration.compareEnable ? VK_TRUE : VK_FALSE;
            createInfo.compareOp = configuration.compareOp;
            createInfo.minLod = configuration.minLod;
            createInfo.maxLod = configuration.maxLod;
            createInfo.borderColor = configuration.borderColor;
            createInfo.unnormalizedCoordinates = VK_FALSE;

            VkResult result = vkCreateSampler(device_->logicalDevice(), &createInfo, nullptr, &sampler_);

            if (result != VK_SUCCESS) {
                COFFEE_ERROR("Failed to create sampler!");

                throw RegularVulkanException { result };
            }
        }

        Sampler::~Sampler() noexcept { vkDestroySampler(device_->logicalDevice(), sampler_, nullptr); }

        SamplerPtr Sampler::create(const DevicePtr& device, const SamplerConfiguration& configuration)
        {
            COFFEE_ASSERT(device != nullptr, "Invalid device provided.");

            return std::shared_ptr<Sampler>(new Sampler { device, configuration });
        }

    } // namespace graphics

} // namespace coffee