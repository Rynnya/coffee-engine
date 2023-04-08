#ifndef COFFEE_VK_DESCRIPTORS
#define COFFEE_VK_DESCRIPTORS

#include <coffee/graphics/buffer.hpp>
#include <coffee/graphics/image.hpp>
#include <coffee/graphics/sampler.hpp>
#include <coffee/objects/texture.hpp>

#include <map>

namespace coffee {

    struct DescriptorBindingInfo {
        VkDescriptorType type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        VkShaderStageFlags shaderStages = 0;
        uint32_t descriptorCount = 1U;
    };

    struct DescriptorWriteInfo {
        VkDescriptorType type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        VkDeviceSize bufferOffset = 0U;
        VkDeviceSize bufferSize = 0U;
        VkBuffer buffer = VK_NULL_HANDLE;
        VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
        ImageView imageView = nullptr;
        Sampler sampler = nullptr;
    };

    class DescriptorLayoutImpl {
    public:
        DescriptorLayoutImpl(Device& device, const std::map<uint32_t, DescriptorBindingInfo>& bindings);
        ~DescriptorLayoutImpl() noexcept;

        inline const VkDescriptorSetLayout& layout() const noexcept
        {
            return layout_;
        }

        const std::map<uint32_t, DescriptorBindingInfo> bindings;

    private:
        Device& device_;

        VkDescriptorSetLayout layout_ = VK_NULL_HANDLE;
    };

    using DescriptorLayout = std::shared_ptr<DescriptorLayoutImpl>;

    class DescriptorWriter {
    public:
        DescriptorWriter(const DescriptorLayout& layout);

        DescriptorWriter(const DescriptorWriter&);
        DescriptorWriter(DescriptorWriter&&) noexcept;
        DescriptorWriter& operator=(const DescriptorWriter&);
        DescriptorWriter& operator=(DescriptorWriter&&) noexcept;

        DescriptorWriter& addBuffer(
            uint32_t bindingIndex,
            const Buffer& buffer,
            size_t offset = 0,
            size_t totalSize = std::numeric_limits<size_t>::max()
        );
        DescriptorWriter& addImage(uint32_t bindingIndex, VkImageLayout layout, const ImageView& image, const Sampler& sampler = nullptr);
        DescriptorWriter& addTexture(uint32_t bindingIndex, const Texture& texture, const Sampler& sampler = nullptr);
        DescriptorWriter& addSampler(uint32_t bindingIndex, const Sampler& sampler);

    private:
        DescriptorLayout layout_;
        std::map<uint32_t, DescriptorWriteInfo> writes_ {};

        friend class DescriptorSetImpl;
    };

    class DescriptorSetImpl : NonMoveable {
    public:
        DescriptorSetImpl(Device& device, const DescriptorWriter& writer);
        ~DescriptorSetImpl() noexcept;

        void updateDescriptor(const DescriptorWriter& writer);

        inline const VkDescriptorSet& set() const noexcept
        {
            return set_;
        }

    private:
        Device& device_;

        VkDescriptorSet set_ = VK_NULL_HANDLE;
    };

    using DescriptorSet = std::shared_ptr<DescriptorSetImpl>;

} // namespace coffee

#endif