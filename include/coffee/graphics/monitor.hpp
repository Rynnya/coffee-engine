#ifndef COFFEE_GRAPHICS_MONITOR
#define COFFEE_GRAPHICS_MONITOR

#include <coffee/interfaces/event_handler.hpp>
#include <coffee/types.hpp>
#include <coffee/utils/non_moveable.hpp>

#include <GLFW/glfw3.h>
#include <volk/volk.h>

#include <any>
#include <memory>
#include <string>
#include <vector>

namespace coffee {

    namespace graphics {

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

        class Monitor;
        using MonitorPtr = std::shared_ptr<const Monitor>;

        class Monitor : NonMoveable {
        public:
            Monitor(GLFWmonitor* handle, uint32_t uniqueID);
            ~Monitor() noexcept = default;

            static MonitorPtr primaryMonitor() noexcept;
            static const std::vector<MonitorPtr>& monitors() noexcept;

            inline static Invokable<const MonitorPtr&> monitorConnectedEvent {};
            inline static Invokable<const MonitorPtr&> monitorDisconnectedEvent {};

            VideoMode currentVideoMode() const noexcept;

            inline const std::vector<VideoMode>& videoModes() const noexcept { return modes_; }

            inline VkExtent2D physicalSize() const noexcept { return physicalSize_; }

            Float2D contentScale() const noexcept;
            VkExtent2D position() const noexcept;
            VkRect2D workArea() const noexcept;

            const uint32_t uniqueID;
            const std::string name;

            mutable std::any userData;

        private:
            // This function will also initialize GLFW, not only monitors
            static void initialize();
            // This function will also deinitialize GLFW, not only monitors
            static void deinitialize() noexcept;

            GLFWmonitor* handle_ = nullptr;
            std::vector<VideoMode> modes_ {};
            VkExtent2D physicalSize_ {};

            inline static uint32_t nextMonitorID_ = 0;
            inline static std::vector<MonitorPtr> monitors_ {};

            friend class Device;
        };

    } // namespace graphics

} // namespace coffee

#endif
