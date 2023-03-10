#include <coffee/abstract/vulkan/vk_backend.hpp>

#include <coffee/abstract/vulkan/vk_buffer.hpp>
#include <coffee/abstract/vulkan/vk_command_buffer.hpp>
#include <coffee/abstract/vulkan/vk_descriptors.hpp>
#include <coffee/abstract/vulkan/vk_pipeline.hpp>
#include <coffee/abstract/vulkan/vk_render_pass.hpp>
#include <coffee/abstract/vulkan/vk_sampler.hpp>
#include <coffee/abstract/vulkan/vk_shader.hpp>

#include <coffee/utils/log.hpp>
#include <coffee/utils/utils.hpp>
#include <coffee/utils/vk_utils.hpp>

namespace coffee {

    VulkanBackend::VulkanBackend(void* windowHandle, uint32_t windowWidth, uint32_t windowHeight)
        : device_ { windowHandle }
        , framebufferWidth_ { windowWidth }
        , framebufferHeight_ { windowHeight }
    {
        swapChain_ = std::make_unique<VulkanSwapChain>(device_, VkExtent2D { windowWidth, windowHeight }, std::nullopt);

        poolsAndBuffers_.resize(swapChain_->getImagesSize());

        checkSupportedPresentModes();
    }

    VulkanBackend::~VulkanBackend() {
        for (const auto& framePoolsAndBuffers : poolsAndBuffers_) {
            for (auto& [pool, commandBuffer] : framePoolsAndBuffers) {
                vkFreeCommandBuffers(device_.getLogicalDevice(), pool, 1U, &commandBuffer);
                device_.returnCommandPool(pool);
            }
        }

        // Required here because otherwise command buffers will be destroyed after device been destroyed
        commandBuffers_.clear();

        swapChain_ = nullptr;
    }

    bool VulkanBackend::acquireFrame() {
        VkResult result = swapChain_->acquireNextImage(currentImage_);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            return false; // Iterate new loop to poll new events
        }

        COFFEE_THROW_IF(result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR, "Failed to acquire swap chain image!");

        for (auto& [pool, commandBuffer] : poolsAndBuffers_[currentImage_]) {
            vkFreeCommandBuffers(device_.getLogicalDevice(), pool, 1U, &commandBuffer);
            device_.returnCommandPool(pool);
        }

        poolsAndBuffers_[currentImage_].clear();

        return true;
    }

    void VulkanBackend::sendCommandBuffer(CommandBuffer&& commandBuffer) {
        std::unique_lock<std::mutex> lock { commandBuffersMutex_ };
        commandBuffers_.push_back(std::move(commandBuffer));
    }

    void VulkanBackend::endFrame() {
        std::vector<VkCommandBuffer> commandBuffersToUpload {};

        for (auto& commandBuffer : commandBuffers_) {
            VulkanCommandBuffer* commandBufferImpl = static_cast<VulkanCommandBuffer*>(commandBuffer.get());

            if (commandBufferImpl->renderPassActive) {
                commandBufferImpl->endRenderPass();
            }

            COFFEE_THROW_IF(
                vkEndCommandBuffer(commandBufferImpl->commandBuffer) != VK_SUCCESS, "Failed to end command buffer recording!");

            commandBuffersToUpload.push_back(commandBufferImpl->commandBuffer);
            poolsAndBuffers_[currentImage_].push_back({ commandBufferImpl->pool, commandBufferImpl->commandBuffer });

            // Take ownership of pool
            commandBufferImpl->pool = nullptr;
        }

        commandBuffers_.clear();
        VkResult result = swapChain_->submitCommandBuffers(commandBuffersToUpload, currentImage_);

        COFFEE_THROW_IF(
            result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR && result != VK_ERROR_OUT_OF_DATE_KHR, "Failed to present swap chain image!");

        currentImage_ = (currentImage_ + 1) % swapChain_->getImagesSize();
    }

    void VulkanBackend::changeFramebufferSize(uint32_t width, uint32_t height) {
        std::unique_ptr<VulkanSwapChain> oldSwapChain = std::move(swapChain_);
        swapChain_ = std::make_unique<VulkanSwapChain>(device_, VkExtent2D { width, height }, oldSwapChain->getVSyncMode(), oldSwapChain);

        COFFEE_THROW_IF(*oldSwapChain != *swapChain_, "SwapChain image or depth format has changed!");

        framebufferWidth_ = width;
        framebufferHeight_ = height;
    }

    void VulkanBackend::changePresentMode(uint32_t width, uint32_t height, PresentMode newMode) {
        VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;

        if (newMode == PresentMode::Immediate) {
            if (immediateSupported_) {
                presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            }

            if (mailboxSupported_) {
                presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
            }
        }

        std::unique_ptr<VulkanSwapChain> oldSwapChain = std::move(swapChain_);
        swapChain_ = std::make_unique<VulkanSwapChain>(device_, VkExtent2D { width, height }, presentMode, oldSwapChain);

        COFFEE_THROW_IF(*oldSwapChain != *swapChain_, "SwapChain image or depth format has changed!");

        framebufferWidth_ = width;
        framebufferHeight_ = height;
    }

    std::unique_ptr<AbstractBuffer> VulkanBackend::createBuffer(const BufferConfiguration& configuration) {
        return std::make_unique<VulkanBuffer>(device_, configuration);
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
        const PushConstants& pushConstants,
        const std::vector<DescriptorLayout>& descriptorLayouts,
        const ShaderProgram& shaderProgram,
        const PipelineConfiguration& configuration
    ) {
        return std::make_unique<VulkanPipeline>(device_, renderPass, pushConstants, descriptorLayouts, shaderProgram, configuration);
    }

    std::unique_ptr<AbstractFramebuffer> VulkanBackend::createFramebuffer(
        const RenderPass& renderPass, 
        const FramebufferConfiguration& configuration
    ) {
        return std::make_unique<VulkanFramebuffer>(device_, renderPass, configuration);
    }

    std::unique_ptr<AbstractCommandBuffer> VulkanBackend::createCommandBuffer() {
        return std::make_unique<VulkanCommandBuffer>(device_);
    }

    void VulkanBackend::copyBuffer(Buffer& dstBuffer, const Buffer& srcBuffer) {
        device_.copyBuffer(srcBuffer, dstBuffer);
    }

    void VulkanBackend::copyBufferToImage(Image& dstImage, const Buffer& srcBuffer) {
        device_.copyBufferToImage(srcBuffer, dstImage);
    }

    Format VulkanBackend::getColorFormat() const noexcept {
        return swapChain_->getImageFormat();
    }

    Format VulkanBackend::getDepthFormat() const noexcept {
        return swapChain_->getDepthFormat();
    }

    uint32_t VulkanBackend::getCurrentImageIndex() const noexcept {
        return currentImage_;
    }

    const std::vector<Image>& VulkanBackend::getPresentImages() const noexcept {
        return swapChain_->getPresentImages();
    }

    void VulkanBackend::waitDevice() {
        vkDeviceWaitIdle(device_.getLogicalDevice());
    }

    void VulkanBackend::checkSupportedPresentModes() noexcept {
        VkUtils::SwapChainSupportDetails swapChainSupport = VkUtils::querySwapChainSupport(device_.getPhysicalDevice(), device_.getSurface());

        mailboxSupported_ = 
            (VkUtils::choosePresentMode(swapChainSupport.presentModes, VK_PRESENT_MODE_MAILBOX_KHR) == VK_PRESENT_MODE_MAILBOX_KHR);
        immediateSupported_ =
            (VkUtils::choosePresentMode(swapChainSupport.presentModes, VK_PRESENT_MODE_IMMEDIATE_KHR) == VK_PRESENT_MODE_IMMEDIATE_KHR);
    }

}