#ifndef COFFEE_ABSTRACT_MONITOR
#define COFFEE_ABSTRACT_MONITOR

#include <coffee/types.hpp>
#include <coffee/utils/non_moveable.hpp>

#include <volk.h>

#include <GLFW/glfw3.h>

#include <any>
#include <memory>
#include <string>
#include <vector>

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
        MonitorImpl(GLFWmonitor* handle, uint32_t uniqueID);
        ~MonitorImpl() noexcept = default;

        VideoMode currentVideoMode() const noexcept;

        inline const std::vector<VideoMode>& videoModes() const noexcept {
            return modes_;
        }

        inline VkExtent2D physicalSize() const noexcept {
            return physicalSize_;
        }

        Float2D contentScale() const noexcept;
        VkExtent2D position() const noexcept;
        VkRect2D workArea() const noexcept;

        const uint32_t uniqueID;
        const std::string name;

        mutable std::any userData;

    private:
        GLFWmonitor* handle_ = nullptr;
        std::vector<VideoMode> modes_ {};
        VkExtent2D physicalSize_ {};
    };

    using Monitor = std::shared_ptr<const MonitorImpl>;

} // namespace coffee

#endif
