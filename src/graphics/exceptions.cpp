#include <coffee/graphics/exceptions.hpp>

#include <coffee/utils/math.hpp>

#include <vulkan/vk_enum_string_helper.h>

#define COFFEE_DECLARE_CONVERT_FUNC(FUNC_NAME, VK_NAME)                                                                                              \
    std::string format::FUNC_NAME(VK_NAME##Flags flags)                                                                                              \
    {                                                                                                                                                \
        if (flags == 0) {                                                                                                                            \
            return "None";                                                                                                                           \
        }                                                                                                                                            \
                                                                                                                                                     \
        std::string result {};                                                                                                                       \
                                                                                                                                                     \
        for (size_t bits = static_cast<size_t>(flags); bits > 0; bits = coffee::Math::excludeLowestBit(bits)) {                                      \
            result.append(string_##VK_NAME##FlagBits(static_cast<VK_NAME##FlagBits>(1U << coffee::Math::getLowestBit(bits)))).append(" |");          \
        }                                                                                                                                            \
                                                                                                                                                     \
        if (result.empty()) {                                                                                                                        \
            return "None";                                                                                                                           \
        }                                                                                                                                            \
                                                                                                                                                     \
        result.resize(result.size() - 2);                                                                                                            \
        return result;                                                                                                                               \
    }

namespace coffee {

COFFEE_DECLARE_CONVERT_FUNC(bufferUsageFlags, VkBufferUsage);
COFFEE_DECLARE_CONVERT_FUNC(imageUsageFlags, VkImageUsage);

const char* VulkanException::resultToErrorMessage(VkResult result) noexcept
{
    switch (result) {
        case VK_SUCCESS:
            return "VK_SUCCESS: Command successfully completed";
        case VK_NOT_READY:
            return "VK_NOT_READY: A fence or query has not yet completed";
        case VK_TIMEOUT:
            return "VK_TIMEOUT: A wait operation has not completed in the specified time";
        case VK_EVENT_SET:
            return "VK_EVENT_SET: An event is signaled";
        case VK_EVENT_RESET:
            return "VK_EVENT_RESET: An event is unsignaled";
        case VK_INCOMPLETE:
            return "VK_INCOMPLETE: A return array was too small for the result";
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            return "VK_ERROR_OUT_OF_HOST_MEMORY: A host memory allocation has failed";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            return "VK_ERROR_OUT_OF_DEVICE_MEMORY: A device memory allocation has failed";
        case VK_ERROR_INITIALIZATION_FAILED:
            return "VK_ERROR_INITIALIZATION_FAILED: Initialization of an object could not be completed for implementation-specific "
                   "reasons";
        case VK_ERROR_DEVICE_LOST:
            return "VK_ERROR_DEVICE_LOST: The logical or physical device has been lost";
        case VK_ERROR_MEMORY_MAP_FAILED:
            return "VK_ERROR_MEMORY_MAP_FAILED: Mapping of a memory object has failed";
        case VK_ERROR_LAYER_NOT_PRESENT:
            return "VK_ERROR_LAYER_NOT_PRESENT: A requested layer is not present or could not be loaded";
        case VK_ERROR_EXTENSION_NOT_PRESENT:
            return "VK_ERROR_EXTENSION_NOT_PRESENT: A requested extension is not supported";
        case VK_ERROR_FEATURE_NOT_PRESENT:
            return "VK_ERROR_FEATURE_NOT_PRESENT: A requested feature is not supported";
        case VK_ERROR_INCOMPATIBLE_DRIVER:
            return "VK_ERROR_INCOMPATIBLE_DRIVER: The requested version of Vulkan is not supported by the driver or is otherwise "
                   "incompatible for implementation-specific reasons";
        case VK_ERROR_TOO_MANY_OBJECTS:
            return "VK_ERROR_TOO_MANY_OBJECTS: Too many objects of the type have already been created";
        case VK_ERROR_FORMAT_NOT_SUPPORTED:
            return "VK_ERROR_FORMAT_NOT_SUPPORTED: A requested format is not supported on this device";
        case VK_ERROR_FRAGMENTED_POOL:
            return "VK_ERROR_FRAGMENTED_POOL: A pool allocation has failed due to fragmentation of the pool's memory";
        case VK_ERROR_OUT_OF_POOL_MEMORY:
            return "VK_ERROR_OUT_OF_POOL_MEMORY: A pool memory allocation has failed";
        case VK_ERROR_INVALID_EXTERNAL_HANDLE:
            return "VK_ERROR_INVALID_EXTERNAL_HANDLE: An external handle is not a valid handle of the specified type";
        case VK_ERROR_FRAGMENTATION:
            return "VK_ERROR_FRAGMENTATION: A descriptor pool creation has failed due to fragmentation";
        case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS:
            return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS: A buffer creation or memory allocation failed because the requested "
                   "address is not available. A shader group handle assignment failed because the requested shader group handle "
                   "information is no longer valid";
        case VK_PIPELINE_COMPILE_REQUIRED:
            return "VK_PIPELINE_COMPILE_REQUIRED: A requested pipeline creation would have required compilation, but the "
                   "application "
                   "requested compilation to not be performed";
        case VK_ERROR_SURFACE_LOST_KHR:
            return "VK_ERROR_SURFACE_LOST_KHR: A surface is no longer available";
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
            return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: The requested window is already in use by Vulkan or another API in a manner "
                   "which prevents it from being used again";
        case VK_SUBOPTIMAL_KHR:
            return "VK_SUBOPTIMAL_KHR: A swapchain no longer matches the surface properties exactly, but can still be used to "
                   "present "
                   "to the surface successfully";
        case VK_ERROR_OUT_OF_DATE_KHR:
            return "VK_ERROR_OUT_OF_DATE_KHR: A surface has changed in such a way that it is no longer compatible with the "
                   "swapchain, "
                   "and further presentation requests using the swapchain will fail";
        case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
            return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: The display used by a swapchain does not use the same presentable image "
                   "layout, "
                   "or is incompatible in a way that prevents sharing an image";
        case VK_THREAD_IDLE_KHR:
            return "VK_THREAD_IDLE_KHR: A deferred operation is not complete but there is currently no work for this thread to do "
                   "at "
                   "the time of this call";
        case VK_THREAD_DONE_KHR:
            return "VK_THREAD_DONE_KHR: A deferred operation is not complete but there is no work remaining to assign to "
                   "additional "
                   "threads";
        case VK_OPERATION_DEFERRED_KHR:
            return "VK_OPERATION_DEFERRED_KHR: A deferred operation was requested and at least some of the work was deferred";
        case VK_OPERATION_NOT_DEFERRED_KHR:
            return "VK_OPERATION_NOT_DEFERRED_KHR: A deferred operation was requested and no operations were deferred";
        case VK_ERROR_UNKNOWN:
        default:
            return "VK_ERROR_UNKNOWN: Unknown Vulkan error";
    }
}

} // namespace coffee

#undef COFFEE_DECLARE_CONVERT_FUNC