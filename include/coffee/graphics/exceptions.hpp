#ifndef COFFEE_GRAPHICS_EXCEPTIONS
#define COFFEE_GRAPHICS_EXCEPTIONS

#include <coffee/utils/platform.hpp>

#include <volk/volk.h>

#include <stdexcept>
#include <string>

namespace coffee {

    class GLFWException : public std::runtime_error {
    public:
        inline GLFWException(const std::string& message) noexcept : std::runtime_error { message } {}
    };

    // Generic class for all Vulkan exceptions
    // This type also contains exceptions from window manager if they appear
    // Never thrown from engine
    class VulkanException : public std::runtime_error {
    public:
        inline VulkanException(VkResult result) noexcept : std::runtime_error { resultToErrorMessage(result) }, result_ { result } {}

        constexpr operator VkResult() const noexcept { return result_; }

        constexpr VkResult result() const noexcept { return result_; }

    private:
        static const char* resultToErrorMessage(VkResult result) noexcept;

        VkResult result_;
    };

    // Vulkan exception that aren't critical and can be recovered from without recreating everything from scratch
    class RegularVulkanException : public VulkanException {
    public:
        inline RegularVulkanException(VkResult result) noexcept : VulkanException { result } {}
    };

    // Vulkan exception that critical and application most likely won't be able to recover from this
    // You can try start recovering by deinitializing and reinitializing engine if resources allow you to do so
    class FatalVulkanException : public VulkanException {
    public:
        inline FatalVulkanException(VkResult result) noexcept : VulkanException { result } {}
    };

} // namespace coffee

#endif
