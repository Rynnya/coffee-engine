#include <coffee/abstract/monitor.hpp>

#include <coffee/utils/log.hpp>

#include <GLFW/glfw3.h>

namespace coffee {

    struct MonitorImpl::PImpl {
        PImpl(void* nativeHandle, uint32_t uniqueID) 
            : monitorHandle { static_cast<GLFWmonitor*>(nativeHandle) }
            , uniqueID { uniqueID }
        {
            int32_t count = 0;
            const GLFWvidmode* videoModes = glfwGetVideoModes(monitorHandle, &count);

            for (int32_t i = 0; i < count; i++) {
                VideoMode videoMode {};
                const GLFWvidmode& nativeMode = videoModes[i];

                videoMode.width = nativeMode.width;
                videoMode.height = nativeMode.height;
                videoMode.depthBits.redChannel = nativeMode.redBits;
                videoMode.depthBits.greenChannel = nativeMode.greenBits;
                videoMode.depthBits.blueChannel = nativeMode.blueBits;
                videoMode.refreshRate = nativeMode.refreshRate;

                modes.push_back(std::move(videoMode));
            }

            int nativeWidth {}, nativeHeight {};
            glfwGetMonitorPhysicalSize(monitorHandle, &nativeWidth, &nativeHeight);
            physicalSize.width = nativeWidth;
            physicalSize.height = nativeHeight;

            name = glfwGetMonitorName(monitorHandle);
        }

        GLFWmonitor* monitorHandle = nullptr;
        std::vector<VideoMode> modes {}; // They don't change through the lifetime of monitor handle
        Extent2D physicalSize {};
        uint32_t uniqueID {};
        std::string name {};
    };

    MonitorImpl::MonitorImpl(void* nativeHandle, uint32_t uniqueID) : pImpl_ { new MonitorImpl::PImpl { nativeHandle, uniqueID } } {
        COFFEE_ASSERT(nativeHandle != nullptr, "Invalid monitor handle provided.");
    }

    MonitorImpl::~MonitorImpl() noexcept {
        delete pImpl_;
    }

    VideoMode MonitorImpl::getCurrentVideoMode() const noexcept {
        VideoMode videoMode {};
        const GLFWvidmode* nativeMode = glfwGetVideoMode(pImpl_->monitorHandle);

        // Cannot throw here, so will just return 0 values, which basically means we cannot get result from monitor
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

    const std::vector<VideoMode>& MonitorImpl::getVideoModes() const noexcept {
        return pImpl_->modes;
    }

    Extent2D MonitorImpl::getPhysicalSize() const noexcept {
        return pImpl_->physicalSize;
    }

    Float2D MonitorImpl::getContentScale() const noexcept {
        Float2D scale {};
        glfwGetMonitorContentScale(pImpl_->monitorHandle, &scale.x, &scale.y);
        return scale;
    }

    Extent2D MonitorImpl::getPosition() const noexcept {
        int32_t nativeWidth {}, nativeHeight {};
        glfwGetMonitorPos(pImpl_->monitorHandle, &nativeWidth, &nativeHeight);
        return { static_cast<uint32_t>(nativeWidth), static_cast<uint32_t>(nativeHeight) };
    }

    WorkArea2D MonitorImpl::getWorkArea() const noexcept {
        int32_t nativeXOffset {}, nativeYOffset {}, nativeWidth {}, nativeHeight {};
        glfwGetMonitorWorkarea(pImpl_->monitorHandle, &nativeXOffset, &nativeYOffset, &nativeWidth, &nativeHeight);
        return { { nativeXOffset, nativeYOffset }, { static_cast<uint32_t>(nativeWidth), static_cast<uint32_t>(nativeHeight) }};
    }

    uint32_t MonitorImpl::getUniqueID() const noexcept {
        return pImpl_->uniqueID;
    }

    const std::string& MonitorImpl::getName() const noexcept {
        return pImpl_->name;
    }

}