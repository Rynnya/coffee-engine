#ifndef COFFEE_VK_WINDOW
#define COFFEE_VK_WINDOW

#include <coffee/abstract/window.hpp>
#include <coffee/abstract/vulkan/vk_device.hpp>

namespace coffee {

    class VulkanWindow : public AbstractWindow {
    public:
        VulkanWindow(VulkanDevice& device, WindowSettings settings, const std::string& windowName);
        ~VulkanWindow() noexcept;

        VkSurfaceKHR surface;

    private:
        VulkanDevice& device_;
    };

}

#endif