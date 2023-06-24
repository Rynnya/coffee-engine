#ifndef COFFEE_GRAPHICS_DESCRIPTORS
#define COFFEE_GRAPHICS_DESCRIPTORS

#include <coffee/graphics/buffer.hpp>
#include <coffee/graphics/image.hpp>
#include <coffee/graphics/sampler.hpp>

#include <map>

namespace coffee {

    namespace graphics {

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
            ImageViewPtr imageView = nullptr;
            SamplerPtr sampler = nullptr;
        };

        class DescriptorLayout;
        using DescriptorLayoutPtr = std::shared_ptr<DescriptorLayout>;

        class DescriptorSet;
        using DescriptorSetPtr = std::shared_ptr<DescriptorSet>;

        class DescriptorLayout {
        public:
            ~DescriptorLayout() noexcept;

            static DescriptorLayoutPtr create(const DevicePtr& device, const std::map<uint32_t, DescriptorBindingInfo>& bindings);

            inline const VkDescriptorSetLayout& layout() const noexcept { return layout_; }

            const std::map<uint32_t, DescriptorBindingInfo> bindings;

        private:
            DescriptorLayout(const DevicePtr& device, const std::map<uint32_t, DescriptorBindingInfo>& bindings);

            DevicePtr device_;

            VkDescriptorSetLayout layout_ = VK_NULL_HANDLE;
        };

        class DescriptorWriter {
        public:
            DescriptorWriter(const DescriptorLayoutPtr& layout);

            DescriptorWriter(const DescriptorWriter&);
            DescriptorWriter(DescriptorWriter&&) noexcept;
            DescriptorWriter& operator=(const DescriptorWriter&);
            DescriptorWriter& operator=(DescriptorWriter&&) noexcept;

            DescriptorWriter& addBuffer(
                uint32_t bindingIndex,
                const BufferPtr& buffer,
                size_t offset = 0,
                size_t totalSize = std::numeric_limits<size_t>::max()
            );
            DescriptorWriter& addImage(
                uint32_t bindingIndex,
                VkImageLayout layout,
                const ImageViewPtr& image,
                const SamplerPtr& sampler = nullptr
            );
            DescriptorWriter& addSampler(uint32_t bindingIndex, const SamplerPtr& sampler);

        private:
            DescriptorLayoutPtr layout_;
            std::map<uint32_t, DescriptorWriteInfo> writes_ {};

            friend class DescriptorSet;
        };

        class DescriptorSet : NonMoveable {
        public:
            ~DescriptorSet() noexcept;

            static DescriptorSetPtr create(const DevicePtr& device, const DescriptorWriter& writer);

            void updateDescriptor(const DescriptorWriter& writer);

            inline const VkDescriptorSet& set() const noexcept { return set_; }

        private:
            DescriptorSet(const DevicePtr& device, const DescriptorWriter& writer);

            DevicePtr device_;

            VkDescriptorSet set_ = VK_NULL_HANDLE;
        };

    } // namespace graphics

} // namespace coffee

#endif