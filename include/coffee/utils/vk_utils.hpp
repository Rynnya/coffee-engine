#ifndef COFFEE_UTILS_VK_UTILS
#define COFFEE_UTILS_VK_UTILS

#include <vulkan/vulkan.h>

#include <coffee/types.hpp>

#include <optional>
#include <vector>

namespace coffee {

    class VkUtils {
    public:
        static VkFormat findSupportedFormat(
            VkPhysicalDevice device, 
            const std::vector<VkFormat>& candidates, 
            VkImageTiling tiling, 
            VkFormatFeatureFlags features);
        static VkFormat findDepthFormat(VkPhysicalDevice device);
        static uint32_t findMemoryType(VkPhysicalDevice device, uint32_t typeFilter, VkMemoryPropertyFlags properties);

        static VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) noexcept;
        static VkPresentModeKHR choosePresentMode(
            const std::vector<VkPresentModeKHR>& availablePresentModes,
            const std::optional<VkPresentModeKHR>& preferable = {}) noexcept;
        static VkExtent2D chooseExtent(const VkExtent2D& extent, const VkSurfaceCapabilitiesKHR& capabilities) noexcept;

        struct SwapChainSupportDetails {
            VkSurfaceCapabilitiesKHR capabilities {};
            std::vector<VkSurfaceFormatKHR> formats {};
            std::vector<VkPresentModeKHR> presentModes {};
        };

        struct QueueFamilyIndices {
            std::optional<uint32_t> graphicsFamily;
            std::optional<uint32_t> presentFamily;

            bool isComplete() const noexcept {
                return graphicsFamily.has_value() && presentFamily.has_value();
            }
        };

        static SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) noexcept;
        static QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) noexcept;

        static uint32_t getOptimalAmountOfFramebuffers(VkPhysicalDevice device, VkSurfaceKHR surface) noexcept;

        static VkDescriptorType getBufferDescriptorType(VkBufferUsageFlags bufferFlags, bool isDynamic = false) noexcept;
        static VkDescriptorType getImageDescriptorType(VkImageUsageFlags imageFlags, bool withSampler = false) noexcept;

        static VkSampleCountFlagBits getUsableSampleCount(uint32_t sampleCount, const VkPhysicalDeviceProperties& properties) noexcept;

        static VkShaderStageFlagBits    transformShaderStage(ShaderStage stage) noexcept;
        static VkShaderStageFlags       transformShaderStages(ShaderStage stages) noexcept;
        static VkBufferUsageFlags       transformBufferFlags(BufferUsage flags) noexcept;
        static VkMemoryPropertyFlags    transformMemoryFlags(MemoryProperty flags) noexcept;
        static VkImageType              transformImageType(ImageType type) noexcept;
        static VkImageViewType          transformImageViewType(ImageViewType type) noexcept;
        static VkImageUsageFlags        transformImageUsage(ImageUsage flags) noexcept;
        static VkImageAspectFlags       transformImageAspects(ImageAspect flags) noexcept;
        static VkImageTiling            transformImageTiling(ImageTiling tiling) noexcept;
        static VkFilter                 transformSamplerFilter(SamplerFilter filter) noexcept;
        static VkSamplerMipmapMode      transformSamplerMipmap(SamplerFilter filter) noexcept;
        static VkFormat                 transformFormat(Format format) noexcept;
        static Format                   transformFormat(VkFormat format) noexcept;
        static VkPrimitiveTopology      transformTopology(Topology topology) noexcept;
        static VkBlendFactor            transformBlendFactor(BlendFactor factor) noexcept;
        static VkBlendOp                transformBlendOp(BlendOp op) noexcept;
        static VkLogicOp                transformLogicOp(LogicOp op) noexcept;
        static VkAttachmentLoadOp       transformAttachmentLoadOp(AttachmentLoadOp op) noexcept;
        static VkAttachmentStoreOp      transformAttachmentStoreOp(AttachmentStoreOp op) noexcept;
        static VkColorComponentFlags    transformColorComponents(ColorComponent components) noexcept;
        static VkComponentSwizzle       transformSwizzleComponent(SwizzleComponent component) noexcept;
        static VkCompareOp              transformCompareOp(CompareOp op) noexcept;
        static VkStencilOp              transformStencilOp(StencilOp op) noexcept;
        static VkCullModeFlags          transformCullMode(CullMode mode) noexcept;
        static VkFrontFace              transformFrontFace(FrontFace face) noexcept;
        static VkPolygonMode            transformPolygonMode(PolygonMode mode) noexcept;
        static VkVertexInputRate        transformInputRate(InputRate inputRate) noexcept;
        static VkSamplerAddressMode     transformAddressMode(AddressMode mode) noexcept;
        static VkBorderColor            transformBorderColor(BorderColor color) noexcept;
        static VkDescriptorType         transformDescriptorType(DescriptorType type) noexcept;
        static VkImageLayout            transformResourceStateToLayout(ResourceState state) noexcept;
        static VkAccessFlags            transformResourceStateToAccess(ResourceState state) noexcept;

        static VkAccessFlags            imageLayoutToAccessFlags(VkImageLayout layout) noexcept;
        static VkPipelineStageFlags     accessFlagsToPipelineStages(VkAccessFlags flags) noexcept;
    };

}

#endif