#ifndef COFFEE_GRAPHICS_DEVICE
#define COFFEE_GRAPHICS_DEVICE

#include <coffee/interfaces/scope_exit.hpp>
#include <coffee/types.hpp>
#include <coffee/utils/non_moveable.hpp>
#include <coffee/utils/vk_utils.hpp>

#include <GLFW/glfw3.h>
#include <oneapi/tbb/concurrent_queue.h>
#include <vma/vk_mem_alloc.h>

#include <array>
#include <mutex>

namespace coffee {

    struct SubmitInfo {
        VkSwapchainKHR swapChain = VK_NULL_HANDLE;
        VkSemaphore waitSemaphone = VK_NULL_HANDLE;
        VkSemaphore signalSemaphone = VK_NULL_HANDLE;
        uint32_t* currentFrame = nullptr;
        std::vector<VkCommandBuffer> commandBuffers {};
    };

    class CommandBuffer;

    // Core class for GPU handling
    // Provides low-level access for Vulkan and mandatory for most graphics wrapper
    // Prefer using wrappers instead of raw Vulkan functions if engine supports such behaviour
    class GPUDevice
        : NonMoveable
        , public std::enable_shared_from_this<GPUDevice> {
    public:
        static constexpr size_t maxOperationsInFlight = 2;

        ~GPUDevice() noexcept;

        static GPUDevicePtr create(VkPhysicalDeviceType preferredDeviceType = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);

        // Called by swapchain when acquiring next image
        // Waits until previous frame is done
        void waitForAcquire();
        // Called by swapchain when resize occurs
        // Waits until all frames in flight is done
        void waitForRelease();

        void sendCommandBuffer(
            CommandBuffer&& commandBuffer,
            VkSwapchainKHR swapChain = VK_NULL_HANDLE,
            VkSemaphore waitSemaphone = VK_NULL_HANDLE,
            VkSemaphore signalSemaphone = VK_NULL_HANDLE,
            uint32_t* currentFrame = nullptr
        );
        void sendCommandBuffers(
            std::vector<CommandBuffer>&& commandBuffers,
            VkSwapchainKHR swapChain = VK_NULL_HANDLE,
            VkSemaphore waitSemaphone = VK_NULL_HANDLE,
            VkSemaphore signalSemaphone = VK_NULL_HANDLE,
            uint32_t* currentFrame = nullptr
        );
        void submitPendingWork();

        inline bool isUnifiedTransferGraphicsQueue() const noexcept { return transferQueue_ == VK_NULL_HANDLE; }

        inline uint32_t transferQueueFamilyIndex() const noexcept
        {
            if (transferQueue_ == VK_NULL_HANDLE) {
                return indices_.transferFamily.value();
            }

            return indices_.graphicsFamily.value();
        }

        inline uint32_t graphicsQueueFamilyIndex() const noexcept { return indices_.graphicsFamily.value(); }

        [[nodiscard]] ScopeExit singleTimeTransfer(std::function<void(const CommandBuffer&)>&& transferActions);
        [[nodiscard]] ScopeExit singleTimeGraphics(std::function<void(const CommandBuffer&)>&& graphicsActions);

        // Generic Vulkan structures

        inline VkInstance instance() const noexcept { return instance_; }

        inline VkPhysicalDevice physicalDevice() const noexcept { return physicalDevice_; }

        inline VkDevice logicalDevice() const noexcept { return logicalDevice_; }

        inline VkDescriptorPool descriptorPool() const noexcept { return descriptorPool_; }

        inline VmaAllocator allocator() const noexcept { return allocator_; }

        // Swapchain related functions

        inline uint32_t imageCount() const noexcept { return imageCountForSwapChain_; }

        inline uint32_t currentOperation() const noexcept { return currentOperation_; }

        inline uint32_t currentOperationInFlight() const noexcept { return currentOperationInFlight_; }

        // Properties

        inline std::array<VmaBudget, VK_MAX_MEMORY_HEAPS> heapBudgets() const noexcept
        {
            std::array<VmaBudget, VK_MAX_MEMORY_HEAPS> budgets {};
            vmaGetHeapBudgets(allocator_, budgets.data());

            return budgets;
        }

        inline const VkPhysicalDeviceMemoryProperties* memoryProperties() const noexcept
        {
            const VkPhysicalDeviceMemoryProperties* properties = nullptr;
            vmaGetMemoryProperties(allocator_, &properties);

            return properties;
        }

        inline const VkPhysicalDeviceProperties& properties() const noexcept { return properties_; }

        // Formats

        inline const VkSurfaceFormatKHR& surfaceFormat() const noexcept { return surfaceFormat_; }

        inline VkFormat optimalDepthFormat() const noexcept { return optimalDepthFormat_; }

        inline VkFormat optimalDepthStencilFormat() const noexcept { return optimalDepthStencilFormat_; }

    private:
        GPUDevice(VkPhysicalDeviceType deviceType);

        void initializeGlobalEnvironment();
        void deinitializeGlobalEnvironment();

        GLFWwindow* createTemporaryWindow();
        VkSurfaceKHR createTemporarySurface(GLFWwindow* window);
        void destroyTemporarySurface(VkSurfaceKHR surface);
        void destroyTemporaryWindow(GLFWwindow* window);

        void createInstance();
        void createDebugMessenger();
        void pickPhysicalDevice(VkSurfaceKHR surface, VkPhysicalDeviceType deviceType);
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

        VkCommandPool acquireGraphicsCommandPool();
        void returnGraphicsCommandPool(VkCommandPool pool);
        VkCommandPool acquireTransferCommandPool();
        void returnTransferCommandPool(VkCommandPool pool);

        void clearCommandBuffers(size_t index);

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
        VkFormat optimalDepthFormat_ = VK_FORMAT_UNDEFINED;
        VkFormat optimalDepthStencilFormat_ = VK_FORMAT_UNDEFINED;
        VkPhysicalDeviceProperties properties_ {};
        VkUtils::QueueFamilyIndices indices_ {};

        VkQueue graphicsQueue_ = VK_NULL_HANDLE;
        VkQueue presentQueue_ = VK_NULL_HANDLE;
        VkQueue transferQueue_ = VK_NULL_HANDLE;
        std::vector<VkFence> operationsInFlight_ {};
        std::array<VkFence, maxOperationsInFlight> inFlightFences_ {};
        std::mutex graphicsQueueMutex_ {};
        std::mutex transferQueueMutex_ {};

        std::mutex submitMutex_ {};
        std::vector<SubmitInfo> pendingSubmits_ {};
        std::vector<bool> poolsAndBuffersClearFlags_ {};
        std::vector<std::vector<std::pair<VkCommandPool, VkCommandBuffer>>> poolsAndBuffers_ {};

        tbb::concurrent_queue<VkCommandPool> graphicsPools_ {};
        tbb::concurrent_queue<VkCommandPool> transferPools_ {};

        VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
        VmaAllocator allocator_ = VK_NULL_HANDLE;

        friend class CommandBuffer;
    };

} // namespace coffee

#endif