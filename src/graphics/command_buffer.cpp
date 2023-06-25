#include <coffee/graphics/command_buffer.hpp>

#include <coffee/graphics/exceptions.hpp>
#include <coffee/utils/log.hpp>

namespace coffee {

    namespace graphics {

        CommandBuffer::CommandBuffer(const DevicePtr& device, CommandBufferType type) : type { type }, device_ { device }
        {
            std::pair<VkCommandPool, VkCommandBuffer> poolAndBuffer { VK_NULL_HANDLE, VK_NULL_HANDLE };

            switch (type) {
                case CommandBufferType::Graphics:
                    poolAndBuffer = device->acquireGraphicsCommandPoolAndBuffer();
                    break;
                case CommandBufferType::Compute:
                    poolAndBuffer = device->acquireComputeCommandPoolAndBuffer();
                    break;
                case CommandBufferType::Transfer:
                    poolAndBuffer = device->acquireTransferCommandPoolAndBuffer();
                    break;
                default:
                    COFFEE_ASSERT(false, "Invalid CommandBufferType provided. Update constructor if new types were added.");
                    break;
            }

            pool_ = poolAndBuffer.first;
            buffer_ = poolAndBuffer.second;

            VkResult result = VK_SUCCESS;
            VkCommandBufferBeginInfo beginInfo { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
            beginInfo.flags = 0;

            if ((result = vkBeginCommandBuffer(buffer_, &beginInfo)) != VK_SUCCESS) {
                COFFEE_ERROR("Failed to begin command buffer recording!");

                throw RegularVulkanException { result };
            }
        }

        CommandBuffer::~CommandBuffer() noexcept
        {
            // Implementation will take ownership of pool when submitting to queue, so pool must be checked
            if (pool_ != VK_NULL_HANDLE) {
                switch (type) {
                    case CommandBufferType::Graphics:
                        device_->returnGraphicsCommandPoolAndBuffer({ pool_, buffer_ });
                        break;
                    case CommandBufferType::Compute:
                        device_->returnComputeCommandPoolAndBuffer({ pool_, buffer_ });
                        break;
                    case CommandBufferType::Transfer:
                        device_->returnTransferCommandPoolAndBuffer({ pool_, buffer_ });
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