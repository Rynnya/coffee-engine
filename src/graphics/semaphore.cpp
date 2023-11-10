#include <coffee/graphics/semaphore.hpp>

#include <coffee/graphics/device.hpp>
#include <coffee/graphics/exceptions.hpp>
#include <coffee/utils/log.hpp>

namespace coffee { namespace graphics {

    Semaphore::Semaphore(const DevicePtr& device) : device_ { device }
    {
        VkSemaphoreCreateInfo createInfo { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        VkResult result = vkCreateSemaphore(device_->logicalDevice(), &createInfo, nullptr, &semaphore_);

        if (result != VK_SUCCESS) {
            COFFEE_ERROR("Failed to create semaphore!");

            throw RegularVulkanException { result };
        }
    }

    Semaphore::~Semaphore() noexcept { vkDestroySemaphore(device_->logicalDevice(), semaphore_, nullptr); }

    SemaphorePtr Semaphore::create(const DevicePtr& device)
    {
        COFFEE_ASSERT(device != nullptr, "Invalid device provided.");

        return std::shared_ptr<Semaphore>(new Semaphore { device });
    }

}} // namespace coffee::graphics