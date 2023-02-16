#ifndef COFFEE_ABSTRACT_BUFFER
#define COFFEE_ABSTRACT_BUFFER

#include <coffee/utils/non_moveable.hpp>
#include <coffee/types.hpp>

namespace coffee {

    class AbstractBuffer : NonMoveable {
    public:
        AbstractBuffer(
            BufferUsage usage,
            MemoryProperty properties,
            uint32_t instanceCount,
            uint32_t instanceSize,
            size_t alignment) noexcept;
        virtual ~AbstractBuffer() noexcept = default;

        template <typename T>
        T* map(size_t size = std::numeric_limits<size_t>::max(), size_t offset = 0ULL) {
            return static_cast<T*>(map(size, offset));
        }

        virtual void* map(size_t size = std::numeric_limits<size_t>::max(), size_t offset = 0ULL) = 0;
        // Flush must be called when CPU write new data into buffer, so GPU can see changes
        virtual void flush(size_t size = std::numeric_limits<size_t>::max(), size_t offset = 0ULL) = 0;
        // Invalidate must be called when GPU write new data into buffer, so CPU can see changes
        virtual void invalidate(size_t size = std::numeric_limits<size_t>::max(), size_t offset = 0ULL) = 0;

        virtual void resize(uint32_t instanceCount, uint32_t instanceSize) = 0;
        virtual void unmap() = 0;

        uint32_t getInstanceCount() const noexcept;
        uint32_t getInstanceSize() const noexcept;
        size_t getAlignment() const noexcept;
        size_t getTotalSize() const noexcept;

        BufferUsage getUsageFlags() const noexcept;
        MemoryProperty getMemoryFlags() const noexcept;

    protected:
        uint32_t instanceCount_;
        uint32_t instanceSize_;
        size_t alignment_;

        BufferUsage usageFlags_;
        MemoryProperty memoryFlags_;
    };

    using Buffer = std::shared_ptr<AbstractBuffer>;

}

#endif