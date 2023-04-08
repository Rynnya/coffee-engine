#ifndef COFFEE_GRAPHICS_DEVICE
#define COFFEE_GRAPHICS_DEVICE

#include <coffee/interfaces/scope_exit.hpp>
#include <coffee/types.hpp>
#include <coffee/utils/non_moveable.hpp>
#include <coffee/utils/vk_utils.hpp>

#include <vk_mem_alloc.h>

#include <array>
#include <mutex>
#include <optional>
#include <vector>

namespace coffee {

    struct SubmitInfo {
        VkSwapchainKHR swapChain = VK_NULL_HANDLE;
        VkSemaphore waitSemaphone = VK_NULL_HANDLE;
        VkSemaphore signalSemaphone = VK_NULL_HANDLE;
        uint32_t* currentFrame = nullptr;
        std::vector<VkCommandBuffer> commandBuffers {};
    };

    // Forward declaration
    class CommandBuffer;

    class Device : NonMoveable {
    public:
        static constexpr size_t maxOperationsInFlight = 2;

        Device();
        ~Device() noexcept;

        VkCommandPool acquireGraphicsCommandPool();
        void returnGraphicsCommandPool(VkCommandPool pool);
        VkCommandPool acquireTransferCommandPool();
        void returnTransferCommandPool(VkCommandPool pool);

        void waitForAcquire();
        void waitForRelease();

        void sendSubmitInfo(SubmitInfo&& submitInfo);
        void submitPendingWork();

        inline bool isUnifiedTransferGraphicsQueue() const noexcept
        {
            return transferQueue_ == VK_NULL_HANDLE;
        }

        inline uint32_t transferQueueFamilyIndex() const noexcept
        {
            if (transferQueue_ == VK_NULL_HANDLE) {
                return indices_.transferFamily.value();
            }

            return indices_.graphicsFamily.value();
        }

        inline uint32_t graphicsQueueFamilyIndex() const noexcept
        {
            return indices_.graphicsFamily.value();
        }

        ScopeExit singleTimeTransfer(std::function<void(const CommandBuffer&)>&& transferActions);
        ScopeExit singleTimeGraphics(std::function<void(const CommandBuffer&)>&& graphicsActions);

        inline VkInstance instance() const noexcept
        {
            return instance_;
        }

        inline VkPhysicalDevice physicalDevice() const noexcept
        {
            return physicalDevice_;
        }

        inline VkDevice logicalDevice() const noexcept
        {
            return logicalDevice_;
        }

        inline VkDescriptorPool descriptorPool() const noexcept
        {
            return descriptorPool_;
        }

        inline VmaAllocator allocator() const noexcept
        {
            return allocator_;
        }

        inline uint32_t imageCount() const noexcept
        {
            return imageCountForSwapChain_;
        }

        inline uint32_t currentOperation() const noexcept
        {
            return currentOperation_;
        }

        inline uint32_t currentOperationInFlight() const noexcept
        {
            return currentOperationInFlight_;
        }

        inline const VkPhysicalDeviceProperties& properties() const noexcept
        {
            return properties_;
        }

        inline const VkSurfaceFormatKHR& surfaceFormat() const noexcept
        {
            return surfaceFormat_;
        }

    private:
        void createInstance();
        void createDebugMessenger();
        void pickPhysicalDevice(VkSurfaceKHR surface);
        void createLogicalDevice(VkSurfaceKHR surface);
        void createSyncObjects();
        void createDescriptorPool();
        void createAllocator();

        bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface, const std::vector<const char*>& additionalExtensions = {});

        std::pair<VkFence, CommandBuffer> beginSingleTimeCommands(CommandBufferType type);
        ScopeExit endSingleTimeCommands(
            CommandBufferType type,
            VkQueue queueToSubmit,
            std::mutex& mutex,
            VkFence fence,
            CommandBuffer&& commandBuffer
        );

        uint32_t imageCountForSwapChain_ = 0;
        uint32_t currentOperation_ = 0;         // Must be in range of [0, imageCountForSwapChain]
        uint32_t currentOperationInFlight_ = 0; // Must be in range of [0, maxOperationsInFlight]

        bool dedicatedAllocationExtensionEnabled = false;
        bool memoryPriorityAndBudgetExtensionsEnabled = false;

        VkInstance instance_ = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT debugMessenger_ = VK_NULL_HANDLE;

        VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
        VkDevice logicalDevice_ = VK_NULL_HANDLE;

        VkSurfaceFormatKHR surfaceFormat_ {};
        VkPhysicalDeviceProperties properties_ {};
        VkUtils::QueueFamilyIndices indices_ {};

        VkQueue graphicsQueue_ = VK_NULL_HANDLE;
        VkQueue presentQueue_ = VK_NULL_HANDLE;
        VkQueue transferQueue_ = VK_NULL_HANDLE;
        std::vector<VkFence> operationsInFlight_ {};
        std::array<VkFence, maxOperationsInFlight> inFlightFences_ {};
        std::mutex graphicsQueueMutex_ {};
        std::mutex transferQueueMutex_ {};

        std::vector<SubmitInfo> pendingSubmits_ {};
        std::vector<VkSwapchainKHR> swapChains_ {};
        std::vector<VkSemaphore> swapChainSemaphores_ {};
        std::vector<uint32_t> imageIndices_ {};

        std::vector<VkCommandPool> graphicsPools_ {};
        std::mutex graphicsPoolsMutex_ {};
        std::vector<VkCommandPool> transferPools_ {};
        std::mutex transferPoolsMutex_ {};

        VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
        VmaAllocator allocator_ = VK_NULL_HANDLE;
    };

} // namespace coffee

#endif