#include <coffee/graphics/fence.hpp>

#include <coffee/graphics/device.hpp>
#include <coffee/graphics/exceptions.hpp>
#include <coffee/utils/log.hpp>

namespace coffee {

namespace graphics {

    Fence::Fence(const DevicePtr& device, bool signaled) : device_ { device }
    {
        VkFenceCreateInfo createInfo { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };

        if (signaled) {
            createInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        }

        VkResult result = vkCreateFence(device_->logicalDevice(), &createInfo, nullptr, &fence_);

        if (result != VK_SUCCESS) {
            COFFEE_ERROR("Failed to create fence to single time command buffer!");

            throw RegularVulkanException { result };
        }
    }

    Fence::~Fence() noexcept
    {
        device_->notifyFenceCleanup(fence_);

        vkDestroyFence(device_->logicalDevice(), fence_, nullptr);
    }

    FencePtr Fence::create(const DevicePtr& device, bool signaled)
    {
        COFFEE_ASSERT(device != nullptr, "Invalid device provided.");

        return std::unique_ptr<Fence>(new Fence { device, signaled });
    }

    VkResult Fence::status() noexcept { return vkGetFenceStatus(device_->logicalDevice(), fence_); }

    void Fence::wait(uint64_t timeout) noexcept { vkWaitForFences(device_->logicalDevice(), 1, &fence_, VK_TRUE, timeout); }

    void Fence::reset() noexcept
    {
        device_->notifyFenceCleanup(fence_);

        vkResetFences(device_->logicalDevice(), 1, &fence_);
    }

} // namespace graphics

} // namespace coffee