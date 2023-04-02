#include <coffee/abstract/vulkan/vk_window.hpp>

#include <coffee/abstract/vulkan/vk_swap_chain.hpp>
#include <coffee/utils/log.hpp>

#include <GLFW/glfw3.h>

namespace coffee {

    VulkanWindow::VulkanWindow(VulkanDevice& device, WindowSettings settings, const std::string& windowName)
            : AbstractWindow { settings, windowName }
            , device_ { device } {
        COFFEE_THROW_IF(
            glfwCreateWindowSurface(device_.getInstance(), static_cast<GLFWwindow*>(windowHandle), nullptr, &surface) != VK_SUCCESS,
            "Failed to create window surface!");

        int32_t framebufferWidth {}, framebufferHeight {};
        glfwGetWindowSize(static_cast<GLFWwindow*>(windowHandle), &framebufferWidth, &framebufferHeight);

        const VkExtent2D extent = { static_cast<uint32_t>(framebufferWidth), static_cast<uint32_t>(framebufferHeight) };
        swapChain = std::make_unique<VulkanSwapChain>(device_, surface, extent, settings.presentMode);
    }

    VulkanWindow::~VulkanWindow() noexcept {
        swapChain->waitIdle();

        swapChain = nullptr;
        vkDestroySurfaceKHR(device_.getInstance(), surface, nullptr);
    }

} // namespace coffee