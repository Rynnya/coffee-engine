#include <coffee/abstract/vulkan/vk_descriptors.hpp>

#include <coffee/abstract/vulkan/vk_buffer.hpp>
#include <coffee/abstract/vulkan/vk_image.hpp>
#include <coffee/abstract/vulkan/vk_sampler.hpp>

#include <coffee/utils/log.hpp>
#include <coffee/utils/vk_utils.hpp>

namespace coffee {

    VulkanDescriptorLayout::VulkanDescriptorLayout(VulkanDevice& device, const std::map<uint32_t, DescriptorBindingInfo>& bindings) 
        : AbstractDescriptorLayout { bindings }
        , device_ { device }
    {
        std::vector<VkDescriptorSetLayoutBinding> bindingsImpl {};

        for (const auto& [index, bindingInfo] : bindings) {
            VkDescriptorSetLayoutBinding bindingImpl {};

            bindingImpl.binding = index;
            bindingImpl.descriptorType = VkUtils::transformDescriptorType(bindingInfo.type);
            bindingImpl.descriptorCount = bindingInfo.descriptorCount;
            bindingImpl.stageFlags = VkUtils::transformShaderStages(bindingInfo.stages);
            bindingImpl.pImmutableSamplers = nullptr;

            bindingsImpl.push_back(std::move(bindingImpl));
        }

        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        descriptorSetLayoutInfo.bindingCount = static_cast<uint32_t>(bindingsImpl.size());
        descriptorSetLayoutInfo.pBindings = bindingsImpl.data();

        COFFEE_THROW_IF(
            vkCreateDescriptorSetLayout(device_.getLogicalDevice(), &descriptorSetLayoutInfo, nullptr, &layout) != VK_SUCCESS,
            "Failed to create descriptor set layout!");
    }

    VulkanDescriptorLayout::~VulkanDescriptorLayout() noexcept {
        vkDestroyDescriptorSetLayout(device_.getLogicalDevice(), layout, nullptr);
    }

    VulkanDescriptorSet::VulkanDescriptorSet(VulkanDevice& device, const DescriptorWriter& writer)
        : AbstractDescriptorSet { getWriterLayout(writer) }
        , device_ { device }
    {
        COFFEE_ASSERT(
            getWriterLayout(writer)->getBindings().size() == getWriterWrites(writer).size(),
            "Layout bindings size ({}) differs from writer bindings size ({}).",
            getWriterLayout(writer)->getBindings().size(),
            getWriterWrites(writer).size());

        VkDescriptorSetAllocateInfo descriptorAllocInfo { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
        descriptorAllocInfo.descriptorPool = device_.getDescriptorPool();
        descriptorAllocInfo.descriptorSetCount = 1;
        descriptorAllocInfo.pSetLayouts = &static_cast<VulkanDescriptorLayout*>(getWriterLayout(writer).get())->layout;
        
        COFFEE_THROW_IF(
            vkAllocateDescriptorSets(device_.getLogicalDevice(), &descriptorAllocInfo, &set) != VK_SUCCESS,
            "Failed to allocate descriptor set!");

        updateDescriptor(writer);
    }

    VulkanDescriptorSet::~VulkanDescriptorSet() noexcept {
        vkFreeDescriptorSets(device_.getLogicalDevice(), device_.getDescriptorPool(), 1, &set);
    }

    void VulkanDescriptorSet::updateDescriptor(const DescriptorWriter& writer) {
        const auto& layout = getWriterLayout(writer);
        const auto& bindings = layout->getBindings();
        const auto& writes = getWriterWrites(writer);

        COFFEE_ASSERT(
            bindings.size() == writes.size(),
            "Layout bindings size ({}) differs from writer bindings size ({}).",
            bindings.size(),
            writes.size());

        std::vector<VkWriteDescriptorSet> writesImpl {};
        std::vector<VkDescriptorBufferInfo> bufferInfos {};
        std::vector<VkDescriptorImageInfo> imageInfos {};

        bufferInfos.reserve(bindings.size());
        imageInfos.reserve(bindings.size());

        for (const auto& [index, bindingInfo] : bindings) {
            VkWriteDescriptorSet writeImpl { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };

            COFFEE_ASSERT(writes.find(index) != writes.end(), "Writer didn't have a requested binding.");
            COFFEE_ASSERT(writes.at(index).type == bindingInfo.type, "Different types requested in same binding.");

            writeImpl.dstSet = set;
            writeImpl.dstBinding = index;
            writeImpl.dstArrayElement = 0U;
            writeImpl.descriptorCount = bindingInfo.descriptorCount;
            writeImpl.descriptorType = VkUtils::transformDescriptorType(bindingInfo.type);

            const auto& writeInfo = writes.at(index);

            switch (bindingInfo.type) {
                case DescriptorType::Sampler: {
                    COFFEE_ASSERT(writeInfo.sampler != nullptr, "Sampler was requested, but wasn't provided.");

                    VkDescriptorImageInfo info {};
                    info.sampler = static_cast<VulkanSampler*>(writeInfo.sampler.get())->sampler;
                    imageInfos.push_back(std::move(info));

                    writeImpl.pImageInfo = &imageInfos.back();
                    break;
                }
                case DescriptorType::ImageAndSampler:
                case DescriptorType::SampledImage: {
                    COFFEE_ASSERT(writeInfo.sampler != nullptr, "Sampler was requested, but wasn't provided.");
                    COFFEE_ASSERT(writeInfo.image != nullptr, "Image was requested, but wasn't provided.");

                    VkDescriptorImageInfo info {};
                    info.sampler = static_cast<VulkanSampler*>(writeInfo.sampler.get())->sampler;
                    info.imageView = static_cast<VulkanImage*>(writeInfo.image.get())->imageView;
                    info.imageLayout = VkUtils::transformResourceStateToLayout(writeInfo.imageState);
                    imageInfos.push_back(std::move(info));

                    writeImpl.pImageInfo = &imageInfos.back();
                    break;
                }
                case DescriptorType::StorageImage:
                case DescriptorType::InputAttachment: {
                    COFFEE_ASSERT(writeInfo.image != nullptr, "Image was requested, but wasn't provided.");

                    VkDescriptorImageInfo info {};
                    info.imageView = static_cast<VulkanImage*>(writeInfo.image.get())->imageView;
                    info.imageLayout = VkUtils::transformResourceStateToLayout(writeInfo.imageState);
                    imageInfos.push_back(std::move(info));

                    writeImpl.pImageInfo = &imageInfos.back();
                    break;
                }
                case DescriptorType::UniformBuffer:
                case DescriptorType::StorageBuffer: {
                    COFFEE_ASSERT(writeInfo.buffer != nullptr, "Buffer was requested, but wasn't provided.");

                    VkDescriptorBufferInfo info {};
                    info.buffer = static_cast<VulkanBuffer*>(writeInfo.buffer.get())->buffer;
                    info.offset = writeInfo.bufferOffset;
                    info.range = writeInfo.bufferSize;
                    bufferInfos.push_back(std::move(info));

                    writeImpl.pBufferInfo = &bufferInfos.back();
                    break;
                }
                default: {
                    COFFEE_ASSERT(false, "Invalid DescriptorType provided in writeInfo. This should not happen.");
                    continue;
                }
            }

            writesImpl.push_back(std::move(writeImpl));
        }

        vkUpdateDescriptorSets(device_.getLogicalDevice(), static_cast<uint32_t>(writesImpl.size()), writesImpl.data(), 0, nullptr);
    }

}