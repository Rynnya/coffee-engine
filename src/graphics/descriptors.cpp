#include <coffee/graphics/descriptors.hpp>

#include <coffee/graphics/exceptions.hpp>
#include <coffee/utils/log.hpp>
#include <coffee/utils/vk_utils.hpp>

namespace coffee {

    namespace graphics {

        DescriptorLayout::DescriptorLayout(const DevicePtr& device, const std::map<uint32_t, DescriptorBindingInfo>& bindings)
            : bindings { bindings }
            , device_ { device }
        {
            std::vector<VkDescriptorSetLayoutBinding> bindingsImpl {};

            for (const auto& [index, bindingInfo] : bindings) {
                VkDescriptorSetLayoutBinding bindingImpl {};

                bindingImpl.binding = index;
                bindingImpl.descriptorType = bindingInfo.type;
                bindingImpl.descriptorCount = bindingInfo.descriptorCount;
                bindingImpl.stageFlags = bindingInfo.shaderStages;
                bindingImpl.pImmutableSamplers = nullptr;

                bindingsImpl.push_back(std::move(bindingImpl));
            }

            VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
            descriptorSetLayoutInfo.bindingCount = static_cast<uint32_t>(bindingsImpl.size());
            descriptorSetLayoutInfo.pBindings = bindingsImpl.data();
            VkResult result = vkCreateDescriptorSetLayout(device_->logicalDevice(), &descriptorSetLayoutInfo, nullptr, &layout_);

            if (result != VK_SUCCESS) {
                COFFEE_ERROR("Failed to create descriptor set layout!");

                throw RegularVulkanException { result };
            }
        }

        DescriptorLayout::~DescriptorLayout() noexcept { vkDestroyDescriptorSetLayout(device_->logicalDevice(), layout_, nullptr); }

        DescriptorLayoutPtr DescriptorLayout::create(const DevicePtr& device, const std::map<uint32_t, DescriptorBindingInfo>& bindings)
        {
            COFFEE_ASSERT(device != nullptr, "Invalid device provided.");

            return std::shared_ptr<DescriptorLayout>(new DescriptorLayout { device, bindings });
        }

        DescriptorWriter::DescriptorWriter(const DescriptorLayoutPtr& layout) : layout_ { layout }
        {
            COFFEE_ASSERT(layout_ != nullptr, "Invalid layout provided.");
        }

        DescriptorWriter::DescriptorWriter(const DescriptorWriter& other) : layout_ { other.layout_ }, writes_ { other.writes_ } {}

        DescriptorWriter::DescriptorWriter(DescriptorWriter&& other) noexcept
            : layout_ { std::move(other.layout_) }
            , writes_ { std::exchange(other.writes_, {}) }
        {}

        DescriptorWriter& DescriptorWriter::operator=(const DescriptorWriter& other)
        {
            if (this == &other) {
                return *this;
            }

            layout_ = other.layout_;
            writes_ = other.writes_;

            return *this;
        }

        DescriptorWriter& DescriptorWriter::operator=(DescriptorWriter&& other) noexcept
        {
            if (this == &other) {
                return *this;
            }

            layout_ = std::move(other.layout_);
            writes_ = std::exchange(other.writes_, {});

            return *this;
        }

        DescriptorWriter& DescriptorWriter::addBuffer(uint32_t bindingIndex, const BufferPtr& buffer, size_t offset, size_t totalSize)
        {
            COFFEE_ASSERT(buffer != nullptr, "Invalid buffer provided.");

            auto binding = layout_->bindings.find(bindingIndex);
            COFFEE_ASSERT(binding != layout_->bindings.end(), "Provided index wasn't found in bindings.");

            const auto& [index, descriptorInfo] = *binding;

            DescriptorWriteInfo write {};
            write.type = descriptorInfo.type;
            write.bufferOffset = offset;
            write.bufferSize = totalSize == std::numeric_limits<size_t>::max() ? buffer->instanceSize * buffer->instanceCount : totalSize;
            write.buffer = buffer->buffer();
            writes_[index] = std::move(write);

            return *this;
        }

        DescriptorWriter& DescriptorWriter::addImage(
            uint32_t bindingIndex,
            VkImageLayout layout,
            const ImageViewPtr& imageView,
            const SamplerPtr& sampler
        )
        {
            COFFEE_ASSERT(imageView != nullptr, "Invalid image view provided.");

            auto binding = layout_->bindings.find(bindingIndex);
            COFFEE_ASSERT(binding != layout_->bindings.end(), "Provided index wasn't found in bindings.");

            const auto& [index, descriptorInfo] = *binding;

            DescriptorWriteInfo write {};
            write.type = descriptorInfo.type;
            write.layout = layout;
            write.imageView = imageView;
            write.sampler = sampler;
            writes_[index] = std::move(write);

            return *this;
        }

        DescriptorWriter& DescriptorWriter::addSampler(uint32_t bindingIndex, const SamplerPtr& sampler)
        {
            COFFEE_ASSERT(sampler != nullptr, "Invalid sampler provided.");

            auto binding = layout_->bindings.find(bindingIndex);
            COFFEE_ASSERT(binding != layout_->bindings.end(), "Provided index wasn't found in bindings.");

            const auto& [index, descriptorInfo] = *binding;

            DescriptorWriteInfo write {};
            write.type = descriptorInfo.type;
            write.sampler = sampler;
            writes_[index] = std::move(write);

            return *this;
        }

        DescriptorSet::DescriptorSet(const DevicePtr& device, const DescriptorWriter& writer) : device_ { device }
        {
            const auto& bindings = writer.layout_->bindings;
            const auto& writes = writer.writes_;
            VkDescriptorSetLayout layout = writer.layout_->layout();

            COFFEE_ASSERT(
                bindings.size() == writes.size(),
                "Layout bindings size ({}) differs from writer bindings size ({}).",
                bindings.size(),
                writes.size()
            );

            VkDescriptorSetAllocateInfo descriptorAllocInfo { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
            descriptorAllocInfo.descriptorPool = device_->descriptorPool();
            descriptorAllocInfo.descriptorSetCount = 1;
            descriptorAllocInfo.pSetLayouts = &layout;
            VkResult result = vkAllocateDescriptorSets(device_->logicalDevice(), &descriptorAllocInfo, &set_);

            if (result != VK_SUCCESS) {
                COFFEE_ERROR("Failed to allocate descriptor set!");

                throw RegularVulkanException { result };
            }

            update(writer);
        }

        DescriptorSet::~DescriptorSet() noexcept { vkFreeDescriptorSets(device_->logicalDevice(), device_->descriptorPool(), 1, &set_); }

        DescriptorSetPtr DescriptorSet::create(const DevicePtr& device, const DescriptorWriter& writer)
        {
            COFFEE_ASSERT(device != nullptr, "Invalid device provided.");

            return std::shared_ptr<DescriptorSet>(new DescriptorSet { device, writer });
        }

        void DescriptorSet::update(const DescriptorWriter& writer)
        {
            const auto& bindings = writer.layout_->bindings;
            const auto& writes = writer.writes_;

            std::vector<VkWriteDescriptorSet> writesImpl {};
            std::vector<VkDescriptorBufferInfo> bufferInfos {};
            std::vector<VkDescriptorImageInfo> imageInfos {};

            bufferInfos.reserve(bindings.size());
            imageInfos.reserve(bindings.size());

            for (const auto& [index, bindingInfo] : bindings) {
                VkWriteDescriptorSet writeImpl { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };

                COFFEE_ASSERT(writes.find(index) != writes.end(), "Writer didn't have a requested binding.");
                COFFEE_ASSERT(writes.at(index).type == bindingInfo.type, "Different types requested in same binding.");

                writeImpl.dstSet = set_;
                writeImpl.dstBinding = index;
                writeImpl.dstArrayElement = 0U;
                writeImpl.descriptorCount = bindingInfo.descriptorCount;
                writeImpl.descriptorType = bindingInfo.type;

                const auto& writeInfo = writes.at(index);

                switch (bindingInfo.type) {
                    case VK_DESCRIPTOR_TYPE_SAMPLER: {
                        COFFEE_ASSERT(writeInfo.sampler != nullptr, "Sampler was requested, but wasn't provided.");

                        VkDescriptorImageInfo info {};
                        info.sampler = writeInfo.sampler->sampler();
                        imageInfos.push_back(std::move(info));

                        writeImpl.pImageInfo = &imageInfos.back();
                        break;
                    }
                    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: {
                        COFFEE_ASSERT(writeInfo.sampler != nullptr, "Sampler was requested, but wasn't provided.");
                        COFFEE_ASSERT(writeInfo.imageView != nullptr, "Image was requested, but wasn't provided.");

                        VkDescriptorImageInfo info {};
                        info.sampler = writeInfo.sampler->sampler();
                        info.imageView = writeInfo.imageView->view();
                        info.imageLayout = writeInfo.layout;
                        imageInfos.push_back(std::move(info));

                        writeImpl.pImageInfo = &imageInfos.back();
                        break;
                    }
                    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
                    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT: {
                        COFFEE_ASSERT(writeInfo.imageView != nullptr, "Image was requested, but wasn't provided.");

                        VkDescriptorImageInfo info {};
                        info.imageView = writeInfo.imageView->view();
                        info.imageLayout = writeInfo.layout;
                        imageInfos.push_back(std::move(info));

                        writeImpl.pImageInfo = &imageInfos.back();
                        break;
                    }
                    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER: {
                        COFFEE_ASSERT(writeInfo.buffer != nullptr, "Buffer was requested, but wasn't provided.");

                        VkDescriptorBufferInfo info {};
                        info.buffer = writeInfo.buffer;
                        info.offset = writeInfo.bufferOffset;
                        info.range = writeInfo.bufferSize;
                        bufferInfos.push_back(std::move(info));

                        writeImpl.pBufferInfo = &bufferInfos.back();
                        break;
                    }
                    default: {
                        COFFEE_ASSERT(false, "Invalid VkDescriptorType provided in writeInfo. This should not happen.");
                        continue;
                    }
                }

                writesImpl.push_back(std::move(writeImpl));
            }

            vkUpdateDescriptorSets(device_->logicalDevice(), static_cast<uint32_t>(writesImpl.size()), writesImpl.data(), 0, nullptr);
        }

    } // namespace graphics

} // namespace coffee