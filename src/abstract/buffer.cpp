#include <coffee/abstract/buffer.hpp>

#include <coffee/utils/log.hpp>

namespace coffee {

    AbstractBuffer::AbstractBuffer(
        BufferUsage usage,
        MemoryProperty properties,
        uint32_t instanceCount,
        uint32_t instanceSize,
        size_t alignment
    ) noexcept
        : instanceCount_ { instanceCount }
        , instanceSize_ { instanceSize }
        , alignment_ { alignment }
        , usageFlags_ { usage }
        , memoryFlags_ { properties }
    {}

    uint32_t AbstractBuffer::getInstanceCount() const noexcept {
        return instanceCount_;
    }

    uint32_t AbstractBuffer::getInstanceSize() const noexcept {
        return instanceSize_;
    }

    size_t AbstractBuffer::getAlignment() const noexcept {
        return alignment_;
    }

    size_t AbstractBuffer::getTotalSize() const noexcept {
        return static_cast<size_t>(alignment_) * instanceCount_;
    }

    BufferUsage AbstractBuffer::getUsageFlags() const noexcept {
        return usageFlags_;
    }

    MemoryProperty AbstractBuffer::getMemoryFlags() const noexcept {
        return memoryFlags_;
    }

}