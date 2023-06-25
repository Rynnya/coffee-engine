#ifndef COFFEE_GRAPHICS_DEVICE
#define COFFEE_GRAPHICS_DEVICE

#include <coffee/interfaces/resource_guard.hpp>
#include <coffee/types.hpp>
#include <coffee/utils/non_moveable.hpp>
#include <coffee/utils/vk_utils.hpp>

#include <GLFW/glfw3.h>
#include <oneapi/tbb/concurrent_queue.h>
#include <vma/vk_mem_alloc.h>
// Volk already included through vk_utils.hpp

#include <array>
#include <mutex>

// Required to remove some annoying warning about redefinition on Windows
#undef APIENTRY

namespace coffee {

    namespace graphics {

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
        class Device
            : NonMoveable
            , public std::enable_shared_from_this<Device> {
        public:
            static constexpr uint32_t kMaxOperationsInFlight = 2;

            ~Device() noexcept;

            static DevicePtr create();

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

            inline uint32_t graphicsQueueFamilyIndex() const noexcept { return indices_.graphicsFamily.value(); }

            inline uint32_t computeQueueFamilyIndex() const noexcept
            {
                if (computeQueue_ != VK_NULL_HANDLE) {
                    return indices_.computeFamily.value();
                }

                return indices_.graphicsFamily.value();
            }

            inline uint32_t transferQueueFamilyIndex() const noexcept
            {
                if (transferQueue_ != VK_NULL_HANDLE) {
                    return indices_.transferFamily.value();
                }

                if (computeQueue_ != VK_NULL_HANDLE) {
                    return indices_.computeFamily.value();
                }

                return indices_.graphicsFamily.value();
            }

            // Returns true when graphics and compute queue from one family; otherwise false (additional synchronization required)
            inline bool isUnifiedGraphicsComputeQueue() const noexcept { return graphicsQueueFamilyIndex() == computeQueueFamilyIndex(); }

            // Returns true when graphics and transfer queue from one family; otherwise false (additional synchronization required)
            inline bool isUnifiedGraphicsTransferQueue() const noexcept { return graphicsQueueFamilyIndex() == transferQueueFamilyIndex(); }

            // Returns true when compute and transfer queue from one family; otherwise false (additional synchronization required)
            inline bool isUnifiedComputeTransferQueue() const noexcept { return computeQueueFamilyIndex() == transferQueueFamilyIndex(); }

            [[nodiscard]] ScopeExit singleTimeTransfer(CommandBuffer&& transferCommandBuffer);
            [[nodiscard]] ScopeExit singleTimeTransfer(std::vector<CommandBuffer>&& transferCommandBuffers);
            [[nodiscard]] ScopeExit singleTimeCompute(CommandBuffer&& computeCommandBuffer);
            [[nodiscard]] ScopeExit singleTimeCompute(std::vector<CommandBuffer>&& computeCommandBuffers);
            [[nodiscard]] ScopeExit singleTimeGraphics(CommandBuffer&& graphicsCommandBuffer);
            [[nodiscard]] ScopeExit singleTimeGraphics(std::vector<CommandBuffer>&& graphicsCommandBuffers);

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

            inline VkFormat surfaceFormat() const noexcept { return surfaceFormat_.format; }

            inline VkColorSpaceKHR surfaceColorSpace() const noexcept { return surfaceFormat_.colorSpace; }

            inline VkFormat optimalDepthFormat() const noexcept { return optimalDepthFormat_; }

            inline VkFormat optimalDepthStencilFormat() const noexcept { return optimalDepthStencilFormat_; }

        private:
            Device();

            void initializeGlobalEnvironment();
            void deinitializeGlobalEnvironment() noexcept;

            GLFWwindow* createTemporaryWindow();
            VkSurfaceKHR createTemporarySurface(GLFWwindow* window);
            void destroyTemporarySurface(VkSurfaceKHR surface);
            void destroyTemporaryWindow(GLFWwindow* window);

            void createInstance();
            void createDebugMessenger();
            void pickPhysicalDevice(VkSurfaceKHR surface);
            void createLogicalDevice(VkSurfaceKHR surface);
            void createSyncObjects();
            void createDescriptorPool();
            void createAllocator();

            bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface, const std::vector<const char*>& additionalExtensions = {});

            VkFence acquireSingleTimeFence();
            ScopeExit endSingleTimeCommands(VkQueue queue, std::mutex& mutex, CommandBuffer&& commandBuffer);
            ScopeExit endSingleTimeCommands(VkQueue queue, std::mutex& mutex, std::vector<CommandBuffer>&& commandBuffers);

            std::pair<VkCommandPool, VkCommandBuffer> acquireGraphicsCommandPoolAndBuffer();
            void returnGraphicsCommandPoolAndBuffer(const std::pair<VkCommandPool, VkCommandBuffer>& buffer);
            std::pair<VkCommandPool, VkCommandBuffer> acquireComputeCommandPoolAndBuffer();
            void returnComputeCommandPoolAndBuffer(const std::pair<VkCommandPool, VkCommandBuffer>& buffer);
            std::pair<VkCommandPool, VkCommandBuffer> acquireTransferCommandPoolAndBuffer();
            void returnTransferCommandPoolAndBuffer(const std::pair<VkCommandPool, VkCommandBuffer>& buffer);

            void clearCommandBuffers(size_t index);

            uint32_t imageCountForSwapChain_ = 0;
            uint32_t currentOperation_ = 0;         // Must be in range of [0, imageCountForSwapChain]
            uint32_t currentOperationInFlight_ = 0; // Must be in range of [0, kMaxOperationsInFlight]

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
            VkQueue computeQueue_ = VK_NULL_HANDLE;
            VkQueue transferQueue_ = VK_NULL_HANDLE;
            std::mutex graphicsQueueMutex_ {};
            std::mutex computeQueueMutex_ {};
            std::mutex transferQueueMutex_ {};

            std::mutex submitMutex_ {};
            std::vector<SubmitInfo> pendingSubmits_ {};
            std::vector<VkFence> operationsInFlight_ {};
            std::array<VkFence, kMaxOperationsInFlight> inFlightFences_ {};
            std::vector<bool> poolsAndBuffersClearFlags_ {};
            std::vector<std::vector<std::pair<VkCommandPool, VkCommandBuffer>>> poolsAndBuffers_ {};

            tbb::concurrent_queue<std::pair<VkCommandPool, VkCommandBuffer>> graphicsPools_ {};
            tbb::concurrent_queue<std::pair<VkCommandPool, VkCommandBuffer>> computePools_ {};
            tbb::concurrent_queue<std::pair<VkCommandPool, VkCommandBuffer>> transferPools_ {};
            tbb::concurrent_queue<VkFence> singleTimeFences_ {};

            VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
            VmaAllocator allocator_ = VK_NULL_HANDLE;

            friend class CommandBuffer;
        };

    } // namespace graphics

} // namespace coffee

#endif