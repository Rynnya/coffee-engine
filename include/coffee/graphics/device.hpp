#ifndef COFFEE_GRAPHICS_DEVICE
#define COFFEE_GRAPHICS_DEVICE

#include <coffee/graphics/fence.hpp>
#include <coffee/graphics/semaphore.hpp>
#include <coffee/interfaces/scope_guard.hpp>
#include <coffee/utils/non_moveable.hpp>
#include <coffee/utils/vk_utils.hpp>

#include <GLFW/glfw3.h>
#include <oneapi/tbb/concurrent_queue.h>
#include <oneapi/tbb/queuing_mutex.h>
#include <oneapi/tbb/task_group.h>
#include <vma/vk_mem_alloc.h>

#include <array>

// Required to remove some annoying warning about redefinition on Windows
#undef APIENTRY

namespace coffee {

    namespace graphics {

        struct SubmitSemaphores {
            std::vector<SemaphorePtr> waitSemaphores = {};
            std::vector<VkPipelineStageFlags> waitDstStageMasks = {};
            std::vector<SemaphorePtr> signalSemaphores = {};
        };

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

            void waitDeviceIdle();
            void waitTransferQueueIdle();
            void waitComputeQueueIdle();
            void waitGraphicsQueueIdle();

            // Submits command buffer to same queue where it was initially created
            // waitAndReset can be set to true ever if you don't provide any fence
            void submit(
                CommandBuffer&& commandBuffer,
                const SubmitSemaphores& semaphores = {},
                const FencePtr& fence = nullptr,
                bool waitAndReset = false
            );
            // Submits command buffers to same queue where it was initially created
            // If command buffer list is empty this operation acts as no-op
            // WARNING: DO NOT MIX command buffers from different queues!
            // waitAndReset can be set to true ever if you don't provide any fence
            void submit(
                std::vector<CommandBuffer>&& commandBuffers,
                const SubmitSemaphores& semaphores = {},
                const FencePtr& fence = nullptr,
                bool waitAndReset = false
            );
            // Presents all swapchain images for current frame and switches to next frame
            // If there's no swapchain work to be done this operation acts as no-op
            void present();

            // This function called by implementation once per imageCount when submit is called
            // You most likely doesn't need to call this function explicitly, unless you want reduce memory footprint
            void clearCompletedWork();

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

            inline const VkPhysicalDeviceMemoryProperties& memoryProperties() const noexcept
            {
                const VkPhysicalDeviceMemoryProperties* properties = nullptr;
                vmaGetMemoryProperties(allocator_, &properties);

                return *properties;
            }

            inline const VkPhysicalDeviceProperties& properties() const noexcept { return properties_; }

            // Formats

            inline VkFormat surfaceFormat() const noexcept { return surfaceFormat_.format; }

            inline VkColorSpaceKHR surfaceColorSpace() const noexcept { return surfaceFormat_.colorSpace; }

            inline VkFormat optimalDepthFormat() const noexcept { return optimalDepthFormat_; }

            inline VkFormat optimalDepthStencilFormat() const noexcept { return optimalDepthStencilFormat_; }

        private:
            struct Task {
                CommandBufferType taskType = CommandBufferType::Graphics;
                VkFence fenceHandle = VK_NULL_HANDLE;
                bool implementationProvidedFence = false;
                uint32_t commandBuffersCount = 0U;
                std::unique_ptr<VkCommandPool[]> commandPools = nullptr;
                std::unique_ptr<VkCommandBuffer[]> commandBuffers = nullptr;
            };

            struct PendingPresent {
                VkSwapchainKHR swapChain = VK_NULL_HANDLE;
                VkSemaphore waitSemaphore = VK_NULL_HANDLE;
                uint32_t* currentFrame = nullptr;
            };

            struct SubmitInfo {
                CommandBufferType submitType = CommandBufferType::Graphics;
                uint32_t commandBuffersCount = 0U;
                std::unique_ptr<VkCommandBuffer[]> commandBuffers = nullptr;
                std::unique_ptr<VkCommandPool[]> commandPools = nullptr;
                uint32_t waitSemaphoresCount = 0U;
                std::unique_ptr<VkSemaphore[]> waitSemaphores = nullptr;
                std::unique_ptr<VkPipelineStageFlags[]> waitDstStageMasks = nullptr;
                uint32_t signalSemaphoresCount = 0U;
                std::unique_ptr<VkSemaphore[]> signalSemaphores = nullptr;
                VkFence fence = VK_NULL_HANDLE;
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
            void createDescriptorPool();
            void createAllocator();

            bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface, const std::vector<const char*>& additionalExtensions = {});

            void translateSemaphores(SubmitInfo& submitInfo, const SubmitSemaphores& semaphores);

            VkFence acquireFence();
            void returnFence(VkFence fence);
            void notifyFenceCleanup(VkFence cleanedFence) noexcept;

            void endGraphicsSubmit(SubmitInfo&& submit, const FencePtr& fence, bool waitAndReset);
            void endComputeSubmit(SubmitInfo&& submit, const FencePtr& fence, bool waitAndReset);
            void endTransferSubmit(SubmitInfo&& submit, const FencePtr& fence, bool waitAndReset);
            void endSubmit(VkQueue queue, tbb::queuing_mutex& mutex, SubmitInfo& submit, VkFence fence);

            std::pair<VkCommandPool, VkCommandBuffer> acquireGraphicsCommandPoolAndBuffer();
            void returnGraphicsCommandPoolAndBuffer(VkCommandPool pool, VkCommandBuffer buffer);
            std::pair<VkCommandPool, VkCommandBuffer> acquireComputeCommandPoolAndBuffer();
            void returnComputeCommandPoolAndBuffer(VkCommandPool pool, VkCommandBuffer buffer);
            std::pair<VkCommandPool, VkCommandBuffer> acquireTransferCommandPoolAndBuffer();
            void returnTransferCommandPoolAndBuffer(VkCommandPool pool, VkCommandBuffer buffer);

            void cleanupCompletedTask(const Task& task);

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

            tbb::queuing_mutex pendingPresentsMutex_ {};
            std::vector<PendingPresent> pendingPresents_ {};

            tbb::queuing_mutex tasksMutex_ {};
            std::vector<Task> runningTasks_ {};

            tbb::concurrent_queue<std::pair<VkCommandPool, VkCommandBuffer>> graphicsPools_ {};
            tbb::concurrent_queue<std::pair<VkCommandPool, VkCommandBuffer>> computePools_ {};
            tbb::concurrent_queue<std::pair<VkCommandPool, VkCommandBuffer>> transferPools_ {};
            tbb::concurrent_queue<VkFence> fencesPool_ {};

            VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
            VmaAllocator allocator_ = VK_NULL_HANDLE;

            friend class CommandBuffer;
            friend class Fence;
            friend class SwapChain;
        };

    } // namespace graphics

} // namespace coffee

#endif