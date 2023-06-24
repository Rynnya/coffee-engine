#include <coffee/graphics/command_buffer.hpp>

#include <coffee/graphics/exceptions.hpp>
#include <coffee/utils/log.hpp>

namespace coffee {

    namespace graphics {

        CommandBuffer::CommandBuffer(const DevicePtr& device, CommandBufferType type) : type { type }, device_ { device }
        {
            switch (type) {
                case CommandBufferType::Graphics:
                    pool_ = device->acquireGraphicsCommandPool();
                    break;
                case CommandBufferType::Compute:
                    pool_ = device->acquireComputeCommandPool();
                    break;
                case CommandBufferType::Transfer:
                    pool_ = device->acquireTransferCommandPool();
                    break;
                default:
                    COFFEE_ASSERT(false, "Invalid CommandBufferType provided. Update constructor if new types were added.");
                    break;
            }

            VkResult result = VK_SUCCESS;

            VkCommandBufferAllocateInfo allocateInfo { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
            allocateInfo.commandPool = pool_;
            allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocateInfo.commandBufferCount = 1;

            if ((result = vkAllocateCommandBuffers(device_->logicalDevice(), &allocateInfo, &buffer_)) != VK_SUCCESS) {
                COFFEE_ERROR("Failed to allocate command buffer!");

                throw RegularVulkanException { result };
            }

            VkCommandBufferBeginInfo beginInfo { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

            if ((result = vkBeginCommandBuffer(buffer_, &beginInfo)) != VK_SUCCESS) {
                COFFEE_ERROR("Failed to begin command buffer recording!");

                throw RegularVulkanException { result };
            }
        }

        CommandBuffer::~CommandBuffer() noexcept
        {
            // Implementation will take ownership of pool when submitting to queue, so pool must be checked
            if (pool_ != VK_NULL_HANDLE) {
                vkFreeCommandBuffers(device_->logicalDevice(), pool_, 1U, &buffer_);

                switch (type) {
                    case CommandBufferType::Graphics:
                        device_->returnGraphicsCommandPool(pool_);
                        break;
                    case CommandBufferType::Compute:
                        device_->returnComputeCommandPool(pool_);
                        break;
                    case CommandBufferType::Transfer:
                        device_->returnTransferCommandPool(pool_);
                        break;
                }
            }
        }

        CommandBuffer CommandBuffer::createGraphics(const DevicePtr& device)
        {
            COFFEE_ASSERT(device != nullptr, "Invalid device provided.");

            return { device, CommandBufferType::Graphics };
        }

        CommandBuffer CommandBuffer::createCompute(const DevicePtr& device)
        {
            COFFEE_ASSERT(device != nullptr, "Invalid device provided.");

            return { device, CommandBufferType::Compute };
        }

        CommandBuffer CommandBuffer::createTransfer(const DevicePtr& device)
        {
            COFFEE_ASSERT(device != nullptr, "Invalid device provided.");

            return { device, CommandBufferType::Transfer };
        }

        CommandBuffer::CommandBuffer(CommandBuffer&& other) noexcept
            : type { other.type }
            , device_ { other.device_ }
            , pool_ { std::exchange(other.pool_, VK_NULL_HANDLE) }
            , buffer_ { std::exchange(other.buffer_, VK_NULL_HANDLE) }
        {}

    } // namespace graphics

} // namespace coffee