#ifndef COFFEE_ABSTRACT_DESCRIPTORS
#define COFFEE_ABSTRACT_DESCRIPTORS

#include <coffee/abstract/buffer.hpp>
#include <coffee/abstract/image.hpp>
#include <coffee/objects/texture.hpp>

#include <map>

namespace coffee {

    class AbstractDescriptorLayout : NonMoveable {
    public:
        AbstractDescriptorLayout(const std::map<uint32_t, DescriptorBindingInfo>& bindings);
        virtual ~AbstractDescriptorLayout() noexcept = default;

        const std::map<uint32_t, DescriptorBindingInfo>& getBindings() const noexcept;

    private:
        const std::map<uint32_t, DescriptorBindingInfo> bindings_;
    };

    using DescriptorLayout = std::shared_ptr<AbstractDescriptorLayout>;

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
        DescriptorWriter& addImage(uint32_t bindingIndex, ResourceState state, const Image& image, const Sampler& sampler = nullptr);
        DescriptorWriter& addTexture(uint32_t bindingIndex, ResourceState state, const Texture& texture, const Sampler& sampler = nullptr);
        DescriptorWriter& addSampler(uint32_t bindingIndex, const Sampler& sampler);

    private:
        DescriptorLayout layout_;
        std::map<uint32_t, DescriptorWriteInfo> writes_ {};

        friend class AbstractDescriptorSet;
    };

    class AbstractDescriptorSet : NonMoveable {
    public:
        AbstractDescriptorSet(const DescriptorLayout& layout) noexcept;
        virtual ~AbstractDescriptorSet() noexcept = default;

        virtual void updateDescriptor(const DescriptorWriter& writer) = 0;

        DescriptorLayout copyLayout() const noexcept;
        const std::map<uint32_t, DescriptorBindingInfo>& getBindings() const noexcept;

    protected:
        static const DescriptorLayout& getWriterLayout(const DescriptorWriter& writer) noexcept;
        static const std::map<uint32_t, DescriptorWriteInfo>& getWriterWrites(const DescriptorWriter& writer) noexcept;

    private:
        DescriptorLayout descriptorLayout_;
    };

    using DescriptorSet = std::shared_ptr<AbstractDescriptorSet>;

} // namespace coffee

#endif