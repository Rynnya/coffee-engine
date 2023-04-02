#ifndef COFFEE_VK_DESCRIPTORS
#define COFFEE_VK_DESCRIPTORS

#include <coffee/abstract/descriptors.hpp>
#include <coffee/abstract/vulkan/vk_device.hpp>

namespace coffee {

    class VulkanDescriptorLayout : public AbstractDescriptorLayout {
    public:
        VulkanDescriptorLayout(VulkanDevice& device, const std::map<uint32_t, DescriptorBindingInfo>& bindings);
        ~VulkanDescriptorLayout() noexcept;

        VkDescriptorSetLayout layout;

    private:
        VulkanDevice& device_;
    };

    class VulkanDescriptorSet : public AbstractDescriptorSet {
    public:
        VulkanDescriptorSet(VulkanDevice& device, const DescriptorWriter& writer);
        ~VulkanDescriptorSet() noexcept;

        void updateDescriptor(const DescriptorWriter& writer) override;

        VkDescriptorSet set;

    private:
        VulkanDevice& device_;
    };

} // namespace coffee

#endif