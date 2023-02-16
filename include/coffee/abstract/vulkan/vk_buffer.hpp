#ifndef COFFEE_VK_BUFFER
#define COFFEE_VK_BUFFER

#include <coffee/abstract/vulkan/vk_device.hpp>
#include <coffee/abstract/buffer.hpp>

#include <coffee/utils/non_copyable.hpp>
#include <coffee/types.hpp>

namespace coffee {

    class VulkanBuffer : public AbstractBuffer {
    public:
        VulkanBuffer(VulkanDevice& device, const BufferConfiguration& configuration);
        ~VulkanBuffer();

        void* map(size_t size, size_t offset) override;
        void flush(size_t size, size_t offset) override;
        void invalidate(size_t size, size_t offset) override;

        void resize(uint32_t instanceCount, uint32_t instanceSize) override;
        void unmap() override;

        VkBuffer buffer = nullptr;
        VkDeviceMemory memory = nullptr;
        void* mappedMemory = nullptr;

    private:
        static size_t calculateAlignment(
            VulkanDevice& device,
            size_t requestedAlignment,
            size_t requiredInstanceSize,
            MemoryProperty properties) noexcept;

        VulkanDevice& device_;
        std::mutex allocationMutex_ {};
    };

}

#endif