#include <coffee/abstract/descriptors.hpp>

#include <coffee/utils/log.hpp>

namespace coffee {

    AbstractDescriptorLayout::AbstractDescriptorLayout(const std::map<uint32_t, DescriptorBindingInfo>& bindings)
        : bindings_ { bindings }
    {}

    const std::map<uint32_t, DescriptorBindingInfo>& AbstractDescriptorLayout::getBindings() const noexcept {
        return bindings_;
    }

    DescriptorWriter::DescriptorWriter(const DescriptorLayout& layout)
        : layout_ { layout }
    {
        COFFEE_ASSERT(layout_ != nullptr, "Invalid layout provided.");
    }

    DescriptorWriter::DescriptorWriter(const DescriptorWriter& other)
        : layout_ { other.layout_ }
        , writes_ { other.writes_ }
    {}

    DescriptorWriter::DescriptorWriter(DescriptorWriter&& other) noexcept
        : layout_ { std::move(other.layout_) }
        , writes_ { std::exchange(other.writes_, {}) }
    {}

    DescriptorWriter& DescriptorWriter::operator=(const DescriptorWriter& other) {
        if (this == &other) {
            return *this;
        }

        layout_ = other.layout_;
        writes_ = other.writes_;

        return *this;
    }

    DescriptorWriter& DescriptorWriter::operator=(DescriptorWriter&& other) noexcept {
        if (this == &other) {
            return *this;
        }

        layout_ = std::move(other.layout_);
        writes_ = std::exchange(other.writes_, {});

        return *this;
    }

    DescriptorWriter& DescriptorWriter::addBuffer(
        uint32_t bindingIndex, 
        const Buffer& buffer,
        size_t offset,
        size_t totalSize
    ) {
        COFFEE_ASSERT(buffer != nullptr, "Invalid buffer provided.");

        const auto& bindings = layout_->getBindings();
        auto binding = bindings.find(bindingIndex);

        COFFEE_ASSERT(binding != bindings.end(), "Provided index wasn't found in bindings.");

        const auto& [index, descriptorInfo] = *binding;

        DescriptorWriteInfo write {};
        write.type = descriptorInfo.type;
        write.bufferOffset = offset;
        write.bufferSize = totalSize == std::numeric_limits<size_t>::max() ? buffer->getTotalSize() : totalSize;
        write.buffer = buffer;
        writes_[index] = std::move(write);

        return *this;
    }

    DescriptorWriter& DescriptorWriter::addImage(
        uint32_t bindingIndex,
        ResourceState state,
        const Image& image, 
        const Sampler& sampler
    ) {
        COFFEE_ASSERT(image != nullptr, "Invalid image provided.");

        [[ maybe_unused ]] constexpr auto verifyLayout = [](ResourceState layout) noexcept -> bool {
            switch (layout) {
                case ResourceState::VertexBuffer:
                case ResourceState::UniformBuffer:
                case ResourceState::IndexBuffer:
                case ResourceState::IndirectCommand:
                case ResourceState::Present:
                    return false;
                default:
                    return true;
            }

            return true;
        };

        const auto& bindings = layout_->getBindings();
        auto binding = bindings.find(bindingIndex);

        COFFEE_ASSERT(verifyLayout(state), "Invalid ResourceState provided.");
        COFFEE_ASSERT(binding != bindings.end(), "Provided index wasn't found in bindings.");

        const auto& [index, descriptorInfo] = *binding;

        DescriptorWriteInfo write {};
        write.type = descriptorInfo.type;
        write.imageState = state;
        write.image = image;
        write.sampler = sampler;
        writes_[index] = std::move(write);

        return *this;
    }

    DescriptorWriter& DescriptorWriter::addTexture(
        uint32_t bindingIndex,
        ResourceState state,
        const Texture& texture,
        const Sampler& sampler
    ) {
        COFFEE_ASSERT(texture != nullptr, "Invalid Texture provided.");
        return this->addImage(bindingIndex, state, texture->texture_, sampler);
    }

    DescriptorWriter& DescriptorWriter::addSampler(uint32_t bindingIndex, const Sampler& sampler) {
        COFFEE_ASSERT(sampler != nullptr, "Invalid sampler provided.");

        const auto& bindings = layout_->getBindings();
        auto binding = bindings.find(bindingIndex);

        COFFEE_ASSERT(binding != bindings.end(), "Provided index wasn't found in bindings.");

        const auto& [index, descriptorInfo] = *binding;

        DescriptorWriteInfo write {};
        write.type = descriptorInfo.type;
        write.sampler = sampler;
        writes_[index] = std::move(write);

        return *this;
    }

    AbstractDescriptorSet::AbstractDescriptorSet(const DescriptorLayout& layout) noexcept
        : descriptorLayout_ { layout }
    {}

    DescriptorLayout AbstractDescriptorSet::copyLayout() const noexcept {
        return descriptorLayout_;
    }

    const std::map<uint32_t, DescriptorBindingInfo>& AbstractDescriptorSet::getBindings() const noexcept {
        return descriptorLayout_->getBindings();
    }

    const DescriptorLayout& AbstractDescriptorSet::getWriterLayout(const DescriptorWriter& writer) noexcept {
        return writer.layout_;
    }

    const std::map<uint32_t, DescriptorWriteInfo>& AbstractDescriptorSet::getWriterWrites(const DescriptorWriter& writer) noexcept {
        return writer.writes_;
    }

}