#ifndef COFFEE_VK_IMAGE
#define COFFEE_VK_IMAGE

#include <coffee/abstract/vulkan/vk_device.hpp>
#include <coffee/abstract/sampler.hpp>

namespace coffee {

    class VulkanImage : public AbstractImage {
    public:
        VulkanImage(VulkanDevice& device, const ImageConfiguration& configuration);
        // Must be used ONLY for swapchain images
        VulkanImage(VulkanDevice& device, uint32_t width, uint32_t height, VkFormat imageFormat, VkImage imageImpl);
        ~VulkanImage() noexcept;

        const bool swapChainImage = false;
        VkImage image;
        VkDeviceMemory memory;
        VkImageView imageView;

        VkSampleCountFlagBits sampleCount;
        VkImageAspectFlags aspectMask;

    private:
        VulkanDevice& device_;
    };

}

#endif