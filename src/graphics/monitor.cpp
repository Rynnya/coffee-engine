#include <coffee/graphics/monitor.hpp>

#include <coffee/graphics/exceptions.hpp>
#include <coffee/utils/log.hpp>

namespace coffee {

    namespace graphics {

        Monitor::Monitor(GLFWmonitor* handle, uint32_t uniqueID) : uniqueID { uniqueID }, name { glfwGetMonitorName(handle) }, handle_ { handle }
        {
            COFFEE_ASSERT(handle_ != nullptr, "Invalid monitor handle provided.");

            int32_t count = 0;
            const GLFWvidmode* videoModes = glfwGetVideoModes(handle_, &count);

            for (int32_t i = 0; i < count; i++) {
                VideoMode videoMode {};
                const GLFWvidmode& nativeMode = videoModes[i];

                videoMode.width = nativeMode.width;
                videoMode.height = nativeMode.height;
                videoMode.depthBits.redChannel = nativeMode.redBits;
                videoMode.depthBits.greenChannel = nativeMode.greenBits;
                videoMode.depthBits.blueChannel = nativeMode.blueBits;
                videoMode.refreshRate = nativeMode.refreshRate;

                modes_.push_back(std::move(videoMode));
            }

            int nativeWidth {}, nativeHeight {};
            glfwGetMonitorPhysicalSize(handle_, &nativeWidth, &nativeHeight);
            physicalSize_.width = nativeWidth;
            physicalSize_.height = nativeHeight;
        }

        MonitorPtr Monitor::primaryMonitor() noexcept { return monitors_.empty() ? nullptr : monitors_[0]; }

        const std::vector<MonitorPtr>& Monitor::monitors() noexcept { return monitors_; }

        VideoMode Monitor::currentVideoMode() const noexcept
        {
            VideoMode videoMode {};
            const GLFWvidmode* nativeMode = glfwGetVideoMode(handle_);

            // Cannot throw here, so will just return 0 values, which basically means we cannot get
            // result from monitor
            if (nativeMode != nullptr) {
                videoMode.width = nativeMode->width;
                videoMode.height = nativeMode->height;
                videoMode.depthBits.redChannel = nativeMode->redBits;
                videoMode.depthBits.greenChannel = nativeMode->greenBits;
                videoMode.depthBits.blueChannel = nativeMode->blueBits;
                videoMode.refreshRate = nativeMode->refreshRate;
            }

            return videoMode;
        }

        Float2D Monitor::contentScale() const noexcept
        {
            Float2D scale {};
            glfwGetMonitorContentScale(handle_, &scale.x, &scale.y);
            return scale;
        }

        VkExtent2D Monitor::position() const noexcept
        {
            int32_t nativeWidth {}, nativeHeight {};
            glfwGetMonitorPos(handle_, &nativeWidth, &nativeHeight);
            return { static_cast<uint32_t>(nativeWidth), static_cast<uint32_t>(nativeHeight) };
        }

        VkRect2D Monitor::workArea() const noexcept
        {
            int32_t nativeXOffset {}, nativeYOffset {}, nativeWidth {}, nativeHeight {};
            glfwGetMonitorWorkarea(handle_, &nativeXOffset, &nativeYOffset, &nativeWidth, &nativeHeight);
            return {
                {nativeXOffset,                       nativeYOffset                      },
                { static_cast<uint32_t>(nativeWidth), static_cast<uint32_t>(nativeHeight)}
            };
        }

        void Monitor::initialize()
        {
            if (glfwInit() != GLFW_TRUE) {
                const char* description = nullptr;
                glfwGetError(&description);

                COFFEE_FATAL("Failed to initialize GLFW! Reason: {}", description);

                throw GLFWException { fmt::format("Failed to initialize GLFW! Reason: {}", description) };
            }

            int32_t monitorCount {};
            GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);

            if (monitors == nullptr) {
                const char* description = nullptr;
                glfwGetError(&description);

                if (description == nullptr) {
                    COFFEE_FATAL("There's no available monitors to display!");

                    throw GLFWException { "There's no available monitors to display!" };
                }

                COFFEE_FATAL("Failed to get monitor handles! Reason: {}", description);

                throw GLFWException { fmt::format("Failed to get monitor handles! Reason: {}", description) };
            }

            glfwSetMonitorCallback([](GLFWmonitor* monitor, int event) {
                if (event == GLFW_CONNECTED) {
                    uint32_t thisMonitorID = nextMonitorID_++;
                    MonitorPtr newMonitor = std::make_shared<Monitor>(monitor, thisMonitorID);

                    monitorConnectedEvent(newMonitor);

                    monitors_.push_back(std::move(newMonitor));
                    glfwSetMonitorUserPointer(monitor, new uint32_t { thisMonitorID });

                    int32_t monitorCount = 0;
                    bool isFirst = glfwGetMonitors(&monitorCount)[0] == monitor;
                    if (isFirst && monitorCount > 1) {
                        // Rotate last monitor and put it into first position
                        std::rotate(monitors_.rbegin(), monitors_.rbegin() + 1, monitors_.rend());
                    }
                }
                else if (event == GLFW_DISCONNECTED) {
                    uint32_t monitorID = *static_cast<uint32_t*>(glfwGetMonitorUserPointer(monitor));

                    for (size_t i = 0; i < monitors_.size(); i++) {
                        auto& monitorPtr = monitors_[i];

                        if (monitorPtr->uniqueID == monitorID) {
                            monitorDisconnectedEvent(monitorPtr);

                            monitors_.erase(monitors_.begin() + i);
                            break;
                        }
                    }

                    delete static_cast<uint32_t*>(glfwGetMonitorUserPointer(monitor));
                }
            });

            for (int32_t i = 0; i < monitorCount; i++) {
                uint32_t thisMonitorID = nextMonitorID_++;
                glfwSetMonitorUserPointer(monitors[i], new uint32_t { thisMonitorID });
                monitors_.push_back(std::make_shared<Monitor>(monitors[i], thisMonitorID));
            }
        }

        void Monitor::deinitialize() noexcept
        {
            for (auto& monitorPtr : monitors_) {
                delete static_cast<uint32_t*>(glfwGetMonitorUserPointer(monitorPtr->handle_));
            }

            glfwTerminate();
        }

    } // namespace graphics

} // namespace coffee