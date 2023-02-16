#ifndef COFFEE_VK_DEVICE
#define COFFEE_VK_DEVICE

#include <coffee/abstract/device.hpp>
#include <coffee/utils/vk_utils.hpp>

#include <mutex>
#include <optional>
#include <vector>

namespace coffee {

    class VulkanDevice : public AbstractDevice {
    public:
        VulkanDevice(void* windowHandle);
        ~VulkanDevice();

        bool isDeviceSuitable(VkPhysicalDevice device, const std::vector<const char*>& additionalExtensions = {});
        bool checkDeviceExtensionsSupport(VkPhysicalDevice device, const std::vector<const char*>& extensions);

        VkCommandPool getUniqueCommandPool();
        void returnCommandPool(VkCommandPool pool);

        void copyBuffer(const Buffer& srcBuffer, const Buffer& dstBuffer);
        void copyBufferToImage(const Buffer& srcBuffer, const Image& dstImage);

        VkInstance getInstance() const noexcept;
        VkPhysicalDevice getPhysicalDevice() const noexcept;
        VkSurfaceKHR getSurface() const noexcept;
        VkDevice getLogicalDevice() const noexcept;
        VkQueue getGraphicsQueue() const noexcept;
        VkQueue getPresentQueue() const noexcept;
        VkDescriptorPool getDescriptorPool() const noexcept;
        const VkPhysicalDeviceProperties& getProperties() const noexcept;

    private:
        void createInstance();
        void createDebugMessenger();
        void createSurface(void* windowHandle);
        void pickPhysicalDevice();
        void createLogicalDevice();
        void createCommandPool();
        void createDescriptorPool();

        VkCommandBuffer beginSingleTimeCommands();
        void endSingleTimeCommands(VkCommandBuffer commandBuffer);

        VkInstance instance_ = nullptr;
        VkSurfaceKHR surface_ = nullptr;
        VkDebugUtilsMessengerEXT debugMessenger_ = nullptr;

        VkPhysicalDevice physicalDevice_ = nullptr;
        VkDevice logicalDevice_ = nullptr;

        VkPhysicalDeviceProperties properties_ {};
        VkUtils::QueueFamilyIndices indices_ {};

        VkQueue graphicsQueue_ = nullptr;
        VkQueue presentQueue_ = nullptr;

        VkCommandPool deviceCommandPool_ = nullptr;
        std::vector<VkCommandPool> pools_ {};
        std::mutex poolsMutex_ {};

        VkDescriptorPool descriptorPool_ = nullptr;
    };

}

#endif