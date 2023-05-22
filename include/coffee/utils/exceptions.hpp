#pragma once

#include <coffee/utils/platform.hpp>

#include <volk/volk.h>

#include <exception>
#include <string>

namespace coffee {

    namespace format {

        std::string bufferUsageFlags(VkBufferUsageFlags flags);
        std::string imageUsageFlags(VkImageUsageFlags flags);

    } // namespace format

    class GLFWException : public std::exception {
    public:
        inline GLFWException(const std::string& message) noexcept : std::exception { message.data() } {};
    };

    // Generic class for all Vulkan exceptions
    // This type also contains exceptions from window manager if they appear
    // Never thrown from engine
    class VulkanException : public std::exception {
    public:
        inline VulkanException(VkResult result) noexcept : std::exception { resultToErrorMessage(result) }, result_ { result } {}

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

    class FilesystemException : public std::exception {
    public:
        const enum class Type {
            ImplementationFailure = 0,
            FileNotFound = 1,
            InvalidFileType = 2,
            InvalidFilesystemSignature = 3,
            BadFilesystemAccess = 4,
            DecompressionFailure = 5
        } type;

        inline FilesystemException(Type type, const std::string& message) noexcept : std::exception { message.data() }, type { type } {};
    };

    class AssetException : public std::exception {
    public:
        const enum class Type {
            ImplementationFailure = 0,
            TypeMismatch = 1,
            NotInCache = 2,
            InvalidFilesystem = 3,
            InvalidRequest = 4
        } type;

        inline AssetException(Type type, const std::string& message) noexcept : std::exception { message.data() }, type { type } {};
    };

} // namespace coffee