#include <coffee/abstract/vulkan/vk_backend.hpp>

#include <coffee/abstract/vulkan/vk_buffer.hpp>
#include <coffee/abstract/vulkan/vk_command_buffer.hpp>
#include <coffee/abstract/vulkan/vk_descriptors.hpp>
#include <coffee/abstract/vulkan/vk_pipeline.hpp>
#include <coffee/abstract/vulkan/vk_render_pass.hpp>
#include <coffee/abstract/vulkan/vk_sampler.hpp>
#include <coffee/abstract/vulkan/vk_shader.hpp>
#include <coffee/abstract/vulkan/vk_window.hpp>

#include <coffee/utils/log.hpp>
#include <coffee/utils/utils.hpp>
#include <coffee/utils/vk_utils.hpp>

namespace coffee {

    VulkanBackend::VulkanBackend() noexcept : AbstractBackend { BackendAPI::Vulkan } {
        poolsAndBuffers_.resize(device_.getSwapChainImageCount());
        poolsAndBuffersClearFlags_.resize(device_.getSwapChainImageCount());

        optimalDepthFormat = VkUtils::transformFormat(VkUtils::findDepthFormat(device_.getPhysicalDevice()));
        surfaceFormat = VkUtils::transformFormat(device_.getSurfaceFormat().format);
    }

    VulkanBackend::~VulkanBackend() noexcept {
        for (const auto& framePoolsAndBuffers : poolsAndBuffers_) {
            for (auto& [pool, commandBuffer] : framePoolsAndBuffers) {
                vkFreeCommandBuffers(device_.getLogicalDevice(), pool, 1U, &commandBuffer);
                device_.returnCommandPool(pool);
            }
        }
    }

    size_t VulkanBackend::getSwapChainImageCount() const noexcept {
        return device_.getSwapChainImageCount();
    }

    Format VulkanBackend::getSurfaceColorFormat() const noexcept {
        return surfaceFormat;
    }

    Format VulkanBackend::getOptimalDepthFormat() const noexcept {
        return optimalDepthFormat;
    }

    Window VulkanBackend::createWindow(WindowSettings settings, const std::string& windowName) {
        return std::make_unique<VulkanWindow>(device_, settings, windowName);
    }

    std::shared_ptr<AbstractBuffer> VulkanBackend::createBuffer(const BufferConfiguration& configuration) {
        return std::make_shared<VulkanBuffer>(device_, configuration);
    }

    std::shared_ptr<AbstractImage> VulkanBackend::createImage(const ImageConfiguration& configuration) {
        return std::make_shared<VulkanImage>(device_, configuration);
    }

    std::shared_ptr<AbstractSampler> VulkanBackend::createSampler(const SamplerConfiguration& configuration) {
        return std::make_shared<VulkanSampler>(device_, configuration);
    }

    std::unique_ptr<AbstractShader> VulkanBackend::createShader(
        const std::string& fileName,
        ShaderStage stage,
        const std::string& entrypoint
    ) {
        return std::make_unique<VulkanShader>(device_, Utils::readFile(fileName), stage, entrypoint);
    }

    std::unique_ptr<AbstractShader> VulkanBackend::createShader(
        const std::vector<uint8_t>& bytes,
        ShaderStage stage,
        const std::string& entrypoint
    ) {
        return std::make_unique<VulkanShader>(device_, bytes, stage, entrypoint);
    }

    std::shared_ptr<AbstractDescriptorLayout> VulkanBackend::createDescriptorLayout(
        const std::map<uint32_t, DescriptorBindingInfo>& bindings
    ) {
        return std::make_shared<VulkanDescriptorLayout>(device_, bindings);
    }

    std::shared_ptr<AbstractDescriptorSet> VulkanBackend::createDescriptorSet(const DescriptorWriter& writer) {
        return std::make_shared<VulkanDescriptorSet>(device_, writer);
    }

    std::unique_ptr<AbstractRenderPass> VulkanBackend::createRenderPass(
        const RenderPass& parent,
        const RenderPassConfiguration& configuration
    ) {
        return std::make_unique<VulkanRenderPass>(device_, configuration, parent);
    }

    std::unique_ptr<AbstractPipeline> VulkanBackend::createPipeline(
        const RenderPass& renderPass,
        const std::vector<DescriptorLayout>& descriptorLayouts,
        const std::vector<Shader>& shaderPrograms,
        const PipelineConfiguration& configuration
    ) {
        return std::make_unique<VulkanPipeline>(device_, renderPass, descriptorLayouts, shaderPrograms, configuration);
    }

    std::unique_ptr<AbstractFramebuffer> VulkanBackend::createFramebuffer(
        const RenderPass& renderPass,
        const FramebufferConfiguration& configuration
    ) {
        return std::make_unique<VulkanFramebuffer>(device_, renderPass, configuration);
    }

    std::unique_ptr<AbstractGraphicsCB> VulkanBackend::createCommandBuffer() {
        return std::make_unique<VulkanGraphicsCB>(device_);
    }

    void VulkanBackend::copyBuffer(Buffer& dstBuffer, const Buffer& srcBuffer) {
        device_.copyBuffer(srcBuffer, dstBuffer);
    }

    void VulkanBackend::copyBufferToImage(Image& dstImage, const Buffer& srcBuffer) {
        device_.copyBufferToImage(srcBuffer, dstImage);
    }

    void VulkanBackend::sendCommandBuffers(std::vector<GraphicsCommandBuffer>&& commandBuffers) {
        std::scoped_lock<std::mutex> lock { commandBufferMutex_ };

        uint32_t currentOperation = device_.getCurrentOperation();
        std::vector<VkCommandBuffer> commandBuffersToUpload {};

        if (poolsAndBuffersClearFlags_[currentOperation]) {
            for (auto& [commandPool, commandBuffer] : poolsAndBuffers_[currentOperation]) {
                vkFreeCommandBuffers(device_.getLogicalDevice(), commandPool, 1, &commandBuffer);
                device_.returnCommandPool(commandPool);
            }

            poolsAndBuffers_[currentOperation].clear();
            poolsAndBuffersClearFlags_[currentOperation] = false;
        }

        for (auto& commandBuffer : commandBuffers) {
            if (commandBuffer == nullptr) {
                continue;
            }

            VulkanGraphicsCB* commandBufferImpl = static_cast<VulkanGraphicsCB*>(commandBuffer.get());

            if (commandBufferImpl->renderPassActive) {
                commandBufferImpl->endRenderPass();
            }

            COFFEE_THROW_IF(
                vkEndCommandBuffer(commandBufferImpl->commandBuffer) != VK_SUCCESS, "Failed to end command buffer recording!");

            commandBuffersToUpload.push_back(commandBufferImpl->commandBuffer);
            poolsAndBuffers_[currentOperation].push_back({ commandBufferImpl->pool, commandBufferImpl->commandBuffer });

            // Take ownership of pool
            commandBufferImpl->pool = nullptr;
        }

        device_.sendCommandBuffers({}, commandBuffersToUpload);
    }

    void VulkanBackend::submitPendingWork() {
        std::scoped_lock<std::mutex> lock { commandBufferMutex_ };

        // We must set this flag before submitting work, otherwise we will lock next vector of
        // command buffers This is something that we don't wanna because it will cause a chaos
        poolsAndBuffersClearFlags_[device_.getCurrentOperation()] = true;

        device_.submitPendingWork();
    }

    void VulkanBackend::waitDevice() {
        vkDeviceWaitIdle(device_.getLogicalDevice());
    }

} // namespace coffee