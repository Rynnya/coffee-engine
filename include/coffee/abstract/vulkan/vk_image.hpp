#ifndef COFFEE_VK_IMAGE
#define COFFEE_VK_IMAGE

#include <coffee/abstract/sampler.hpp>
#include <coffee/abstract/vulkan/vk_device.hpp>

namespace coffee {

    class VulkanImage : public AbstractImage {
    public:
        VulkanImage(VulkanDevice& device, const ImageConfiguration& configuration);
        // Must be used ONLY for swapchain images
        VulkanImage(VulkanDevice& device, uint32_t width, uint32_t height, VkFormat imageFormat, VkImage imageImpl);
        ~VulkanImage() noexcept;

        void setNewLayout(ResourceState newLayout) noexcept;

        const bool swapChainImage = false;
        VkImage image = nullptr;
        VkDeviceMemory memory = nullptr;
        VkImageView imageView = nullptr;

        VkSampleCountFlagBits sampleCount;
        VkImageAspectFlags aspectMask;

    private:
        VulkanDevice& device_;
    };

} // namespace coffee

#endif