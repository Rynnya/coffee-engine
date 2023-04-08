#ifndef COFFEE_UTILS_VK_UTILS
#define COFFEE_UTILS_VK_UTILS

#include <volk.h>

#include <optional>
#include <vector>

namespace coffee {

    class VkUtils {
    public:
        static VkFormat findSupportedFormat(
            VkPhysicalDevice device,
            const std::vector<VkFormat>& candidates,
            VkImageTiling tiling,
            VkFormatFeatureFlags features
        );
        static VkFormat findDepthFormat(VkPhysicalDevice device);
        static uint32_t findMemoryType(VkPhysicalDevice device, uint32_t typeFilter, VkMemoryPropertyFlags properties);

        static VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) noexcept;
        static VkPresentModeKHR choosePresentMode(
            const std::vector<VkPresentModeKHR>& availablePresentModes,
            VkPresentModeKHR preferable
        ) noexcept;
        static VkExtent2D chooseExtent(const VkExtent2D& extent, const VkSurfaceCapabilitiesKHR& capabilities) noexcept;

        struct SwapChainSupportDetails {
            VkSurfaceCapabilitiesKHR capabilities {};
            std::vector<VkSurfaceFormatKHR> formats {};
            std::vector<VkPresentModeKHR> presentModes {};
        };

        struct QueueFamilyIndices {
            std::optional<uint32_t> graphicsFamily = std::nullopt;
            std::optional<uint32_t> presentFamily = std::nullopt;
            std::optional<uint32_t> transferFamily = std::nullopt;

            inline bool isComplete() const noexcept
            {
                return graphicsFamily.has_value() && presentFamily.has_value() && transferFamily.has_value();
            }
        };

        static std::vector<VkExtensionProperties> getInstanceExtensions();
        static std::vector<VkExtensionProperties> getDeviceExtensions(VkPhysicalDevice device);
        static bool isExtensionsAvailable(
            const std::vector<VkExtensionProperties>& availableExtensions,
            const std::vector<const char*>& requiredExtensions
        ) noexcept;

        static SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) noexcept;
        static QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) noexcept;

        static uint32_t getOptimalAmountOfFramebuffers(VkPhysicalDevice device, VkSurfaceKHR surface) noexcept;

        static VkDescriptorType getBufferDescriptorType(VkBufferUsageFlags bufferFlags, bool isDynamic = false) noexcept;
        static VkDescriptorType getImageDescriptorType(VkImageUsageFlags imageFlags, bool withSampler = false) noexcept;

        static VkSampleCountFlagBits getUsableSampleCount(
            VkSampleCountFlagBits sampleCount,
            const VkPhysicalDeviceProperties& properties
        ) noexcept;
    };

} // namespace coffee

#endif