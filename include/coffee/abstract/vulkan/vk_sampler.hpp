#ifndef COFFEE_VK_SAMPLER
#define COFFEE_VK_SAMPLER

#include <coffee/abstract/vulkan/vk_device.hpp>
#include <coffee/types.hpp>

namespace coffee {

    class VulkanSampler : public AbstractSampler {
    public:
        VulkanSampler(VulkanDevice& device, const SamplerConfiguration& configuration);
        ~VulkanSampler() noexcept;

        VkSampler sampler;

    private:
        VulkanDevice& device_;
    };

} // namespace coffee

#endif