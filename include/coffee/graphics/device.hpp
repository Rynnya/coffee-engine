#ifndef COFFEE_GRAPHICS_DEVICE
#define COFFEE_GRAPHICS_DEVICE

#include <coffee/graphics/semaphore.hpp>
#include <coffee/interfaces/resource_guard.hpp>
#include <coffee/utils/non_moveable.hpp>
#include <coffee/utils/vk_utils.hpp>

#include <GLFW/glfw3.h>
#include <oneapi/tbb/concurrent_queue.h>
#include <oneapi/tbb/queuing_mutex.h>
#include <oneapi/tbb/task_group.h>
#include <vma/vk_mem_alloc.h>

// Volk already included through vk_utils.hpp

#include <array>

// Required to remove some annoying warning about redefinition on Windows
#undef APIENTRY

namespace coffee {

    namespace graphics {

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
                const std::vector<SemaphorePtr>& waitSemaphores = {},
                const std::vector<VkPipelineStageFlags>& waitDstStageMasks = {},
                const std::vector<SemaphorePtr>& signalSemaphores = {}
            );
            void sendCommandBuffers(
                CommandBufferType submitType,
                std::vector<CommandBuffer>&& commandBuffers,
                const std::vector<SemaphorePtr>& waitSemaphores = {},
                const std::vector<VkPipelineStageFlags>& waitDstStageMasks = {},
                const std::vector<SemaphorePtr>& signalSemaphores = {}
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
            struct RunningCommandBuffer {
                CommandBufferType commandBufferType = CommandBufferType::Graphics;
                VkCommandPool pool = VK_NULL_HANDLE;
                VkCommandBuffer buffer = VK_NULL_HANDLE;
            };

            struct RunningGPUTask {
                RunningGPUTask() noexcept = default;

                RunningGPUTask(const RunningGPUTask&) noexcept = delete;
                RunningGPUTask& operator=(const RunningGPUTask&) noexcept = delete;

                RunningGPUTask(RunningGPUTask&& other) noexcept
                    : graphicsCompletionFence { std::exchange(other.graphicsCompletionFence, VK_NULL_HANDLE) }
                    , computeCompletionFence { std::exchange(other.computeCompletionFence, VK_NULL_HANDLE) }
                    , transferCompletionFence { std::exchange(other.transferCompletionFence, VK_NULL_HANDLE) }
                    , commandBuffers { std::move(other.commandBuffers) }
                {}

                RunningGPUTask& operator=(RunningGPUTask&& other) noexcept
                {
                    if (this == &other) {
                        return *this;
                    }

                    graphicsCompletionFence = std::exchange(other.graphicsCompletionFence, VK_NULL_HANDLE);
                    computeCompletionFence = std::exchange(other.computeCompletionFence, VK_NULL_HANDLE);
                    transferCompletionFence = std::exchange(other.transferCompletionFence, VK_NULL_HANDLE);
                    commandBuffers = std::move(other.commandBuffers);

                    return *this;
                }

                void clear() noexcept
                {
                    graphicsCompletionFence = VK_NULL_HANDLE;
                    computeCompletionFence = VK_NULL_HANDLE;
                    transferCompletionFence = VK_NULL_HANDLE;

                    commandBuffers.clear();
                }

                VkFence graphicsCompletionFence = VK_NULL_HANDLE;
                VkFence computeCompletionFence = VK_NULL_HANDLE;
                VkFence transferCompletionFence = VK_NULL_HANDLE;
                std::vector<RunningCommandBuffer> commandBuffers {};
            };

            struct SubmitInfo {
                CommandBufferType submitType = CommandBufferType::Graphics;
                uint32_t commandBuffersCount = 0U;
                uint32_t waitSemaphoresCount = 0U;
                uint32_t signalSemaphoresCount = 0U;
                std::unique_ptr<VkCommandBuffer[]> commandBuffers = nullptr;
                std::unique_ptr<VkSemaphore[]> waitSemaphores = nullptr;
                std::unique_ptr<VkPipelineStageFlags[]> waitDstStageMasks = nullptr;
                std::unique_ptr<VkSemaphore[]> signalSemaphores = nullptr;
                VkSwapchainKHR swapChain = VK_NULL_HANDLE;
                VkSemaphore swapChainWaitSemaphore = VK_NULL_HANDLE;
                uint32_t* currentFrame = nullptr;
            };

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

            void translateSemaphores(
                SubmitInfo& submitInfo,
                const std::vector<SemaphorePtr>& waitSemaphores,
                const std::vector<VkPipelineStageFlags>& waitDstStageMasks,
                const std::vector<SemaphorePtr>& signalSemaphores
            );
            void transferSubmitInfo(SubmitInfo&& submitInfo, std::vector<CommandBuffer>&& commandBuffers);

            VkFence acquireFence();
            void returnFence(VkFence fence);

            ScopeExit endSingleTimeCommands(VkQueue queue, tbb::queuing_mutex& mutex, CommandBuffer&& commandBuffer);
            ScopeExit endSingleTimeCommands(VkQueue queue, tbb::queuing_mutex& mutex, std::vector<CommandBuffer>&& commandBuffers);
            void endSubmit(VkQueue queue, tbb::queuing_mutex& mutex, const std::vector<VkSubmitInfo>& submits, VkFence fence);

            std::pair<VkCommandPool, VkCommandBuffer> acquireGraphicsCommandPoolAndBuffer();
            void returnGraphicsCommandPoolAndBuffer(VkCommandPool pool, VkCommandBuffer buffer);
            std::pair<VkCommandPool, VkCommandBuffer> acquireComputeCommandPoolAndBuffer();
            void returnComputeCommandPoolAndBuffer(VkCommandPool pool, VkCommandBuffer buffer);
            std::pair<VkCommandPool, VkCommandBuffer> acquireTransferCommandPoolAndBuffer();
            void returnTransferCommandPoolAndBuffer(VkCommandPool pool, VkCommandBuffer buffer);

            static bool isCommandBufferAllowedForSubmitType(CommandBufferType submitType, CommandBufferType commandBufferType);
            void clearCompletedCommandBuffers();

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
            tbb::queuing_mutex graphicsQueueMutex_ {};
            tbb::queuing_mutex computeQueueMutex_ {};
            tbb::queuing_mutex transferQueueMutex_ {};

            tbb::queuing_mutex submitMutex_ {};
            std::vector<SubmitInfo> pendingSubmits_ {};

            std::atomic_bool cleanupRunning_ {};
            tbb::queuing_mutex cleanupMutex_ {};
            std::thread cleanupThread_ {};
            RunningGPUTask pendingTask_ {};
            std::vector<RunningGPUTask> runningTasks_ {};

            std::vector<VkFence> operationsInFlight_ {};
            std::array<VkFence, kMaxOperationsInFlight> fencesInFlight_ {};

            tbb::concurrent_queue<std::pair<VkCommandPool, VkCommandBuffer>> graphicsPools_ {};
            tbb::concurrent_queue<std::pair<VkCommandPool, VkCommandBuffer>> computePools_ {};
            tbb::concurrent_queue<std::pair<VkCommandPool, VkCommandBuffer>> transferPools_ {};
            tbb::concurrent_queue<VkFence> fencesPool_ {};

            VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
            VmaAllocator allocator_ = VK_NULL_HANDLE;

            friend class CommandBuffer;
            friend class SwapChain;
        };

    } // namespace graphics

} // namespace coffee

#endif