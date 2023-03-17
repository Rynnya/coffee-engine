#ifndef COFFEE_ABSTRACT_MONITOR
#define COFFEE_ABSTRACT_MONITOR

#include <coffee/utils/non_moveable.hpp>
#include <coffee/types.hpp>

#include <any>

namespace coffee {

    struct DepthBits {
        uint32_t redChannel = 0;
        uint32_t greenChannel = 0;
        uint32_t blueChannel = 0;
    };

    struct VideoMode {
        uint32_t width = 0;
        uint32_t height = 0;
        DepthBits depthBits {};
        uint32_t refreshRate = 0;
    };

    class MonitorImpl : NonMoveable {
    public:
        MonitorImpl(void* nativeHandle, uint32_t uniqueID);
        ~MonitorImpl() noexcept;

        VideoMode getCurrentVideoMode() const noexcept;
        const std::vector<VideoMode>& getVideoModes() const noexcept;

        Extent2D getPhysicalSize() const noexcept;
        Float2D getContentScale() const noexcept;
        Extent2D getPosition() const noexcept;
        WorkArea2D getWorkArea() const noexcept;

        uint32_t getUniqueID() const noexcept;
        const std::string& getName() const noexcept;

        mutable std::any userData;

    private:
        struct PImpl;
        PImpl* pImpl_;
    };

    using Monitor = std::shared_ptr<const MonitorImpl>;

}

#endif
