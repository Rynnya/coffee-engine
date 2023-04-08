#include <coffee/graphics/monitor.hpp>

#include <coffee/utils/log.hpp>

namespace coffee {

    MonitorImpl::MonitorImpl(GLFWmonitor* handle, uint32_t uniqueID)
        : uniqueID { uniqueID }
        , name { glfwGetMonitorName(handle) }
        , handle_ { handle }
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

    VideoMode MonitorImpl::currentVideoMode() const noexcept
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

    Float2D MonitorImpl::contentScale() const noexcept
    {
        Float2D scale {};
        glfwGetMonitorContentScale(handle_, &scale.x, &scale.y);
        return scale;
    }

    VkExtent2D MonitorImpl::position() const noexcept
    {
        int32_t nativeWidth {}, nativeHeight {};
        glfwGetMonitorPos(handle_, &nativeWidth, &nativeHeight);
        return { static_cast<uint32_t>(nativeWidth), static_cast<uint32_t>(nativeHeight) };
    }

    VkRect2D MonitorImpl::workArea() const noexcept
    {
        int32_t nativeXOffset {}, nativeYOffset {}, nativeWidth {}, nativeHeight {};
        glfwGetMonitorWorkarea(handle_, &nativeXOffset, &nativeYOffset, &nativeWidth, &nativeHeight);
        return {
            {nativeXOffset,                       nativeYOffset                      },
            { static_cast<uint32_t>(nativeWidth), static_cast<uint32_t>(nativeHeight)}
        };
    }

} // namespace coffee