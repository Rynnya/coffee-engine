#ifndef COFFEE_VK_DEVICE
#define COFFEE_VK_DEVICE

#include <coffee/abstract/device.hpp>
#include <coffee/utils/vk_utils.hpp>

#include <array>
#include <mutex>
#include <optional>
#include <vector>

namespace coffee {

    struct SwapChainSubmit {
        VkSemaphore waitSemaphone = nullptr;
        VkSemaphore signalSemaphone = nullptr;
        uint32_t* currentFrame = nullptr;
        VkSwapchainKHR swapChain = nullptr;
    };

    struct SubmitInfo {
        SwapChainSubmit swapChainSubmit;
        std::vector<VkCommandBuffer> commandBuffers;
    };

    class VulkanDevice : public AbstractDevice {
    public:
        VulkanDevice();
        ~VulkanDevice();

        bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface, const std::vector<const char*>& additionalExtensions = {});
        bool checkDeviceExtensionsSupport(VkPhysicalDevice device, const std::vector<const char*>& extensions);

        VkCommandPool getUniqueCommandPool();
        void returnCommandPool(VkCommandPool pool);

        void copyBuffer(const Buffer& srcBuffer, const Buffer& dstBuffer);
        void copyBufferToImage(const Buffer& srcBuffer, const Image& dstImage);

        void waitForAcquire() override;
        void sendCommandBuffers(const SwapChainSubmit& swapChainSubmit, const std::vector<VkCommandBuffer>& commandBuffers);
        void submitPendingWork();

        VkInstance getInstance() const noexcept;
        VkPhysicalDevice getPhysicalDevice() const noexcept;
        VkDevice getLogicalDevice() const noexcept;
        VkQueue getGraphicsQueue() const noexcept;
        VkQueue getPresentQueue() const noexcept;
        const std::array<VkFence, maxOperationsInFlight>& getQueueFences() const noexcept;
        VkDescriptorPool getDescriptorPool() const noexcept;

        const VkPhysicalDeviceProperties& getProperties() const noexcept;
        const VkSurfaceFormatKHR& getSurfaceFormat() const noexcept;

    private:
        void createInstance();
        void createDebugMessenger();
        void createTemporarySurface();
        void pickPhysicalDevice(VkSurfaceKHR surface);
        void createLogicalDevice(VkSurfaceKHR surface);
        void createSyncObjects();
        void createCommandPool();
        void createDescriptorPool();
        void destroyTemporarySurface();

        VkCommandBuffer beginSingleTimeCommands();
        void endSingleTimeCommands(VkCommandBuffer commandBuffer);

        VkInstance instance_ = nullptr;
        VkSurfaceFormatKHR surfaceFormat_ {};
        VkDebugUtilsMessengerEXT debugMessenger_ = nullptr;

        VkPhysicalDevice physicalDevice_ = nullptr;
        VkDevice logicalDevice_ = nullptr;

        VkPhysicalDeviceProperties properties_ {};
        VkUtils::QueueFamilyIndices indices_ {};

        VkQueue graphicsQueue_ = nullptr;
        VkQueue presentQueue_ = nullptr;
        std::vector<VkFence> operationsInFlight_ {};
        std::array<VkFence, AbstractDevice::maxOperationsInFlight> inFlightFences_ {};
        std::mutex queueMutex_ {};

        std::vector<SubmitInfo> pendingSubmits_ {};
        std::vector<VkSwapchainKHR> swapChains_ {};
        std::vector<VkSemaphore> swapChainSemaphores_ {};
        std::vector<uint32_t> imageIndices_ {};

        VkCommandPool deviceCommandPool_ = nullptr;
        std::vector<VkCommandPool> pools_ {};
        std::mutex poolsMutex_ {};

        VkDescriptorPool descriptorPool_ = nullptr;
    };

} // namespace coffee

#endif