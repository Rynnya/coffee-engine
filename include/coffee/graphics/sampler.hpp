#ifndef COFFEE_VK_SAMPLER
#define COFFEE_VK_SAMPLER

#include <coffee/graphics/device.hpp>

namespace coffee {

    struct SamplerConfiguration {
        VkFilter magFilter = VK_FILTER_NEAREST;
        VkFilter minFilter = VK_FILTER_NEAREST;
        VkSamplerMipmapMode mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        VkSamplerAddressMode addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        VkSamplerAddressMode addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        VkSamplerAddressMode addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        float mipLodBias = 0.0f;
        bool anisotropyEnable = false;
        float maxAnisotropy = 1.0f;
        bool compareEnable = false;
        VkCompareOp compareOp = VK_COMPARE_OP_NEVER;
        float minLod = 0.0f;
        float maxLod = 0.0f;
        VkBorderColor borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    };

    class SamplerImpl {
    public:
        SamplerImpl(Device& device, const SamplerConfiguration& configuration);
        ~SamplerImpl() noexcept;

        inline const VkSampler& sampler() const noexcept
        {
            return sampler_;
        }

    private:
        Device& device_;

        VkSampler sampler_ = VK_NULL_HANDLE;
    };

    using Sampler = std::shared_ptr<SamplerImpl>;

} // namespace coffee

#endif