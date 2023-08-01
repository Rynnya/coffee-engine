#include <coffee/interfaces/asset_manager.hpp>

#include <coffee/graphics/command_buffer.hpp>
#include <coffee/graphics/vertex.hpp>
#include <coffee/interfaces/exceptions.hpp>
#include <coffee/utils/utils.hpp>

#include <basis_universal/basisu_transcoder.h>
#include <oneapi/tbb/parallel_for.h>
#include <xxh3/xxhash.h>

namespace coffee {

    namespace detail {

        constexpr const char kBasisChannelCountField[] = "CFAchannelCount";

        constexpr const char kMissingTextureBytes[] =
            "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8"
            "\xf8\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8"
            "\xf8\xf8\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8"
            "\xf8\xf8\xf8\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8"
            "\xf8\xf8\xf8\xf8\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8"
            "\xf8\xf8\xf8\xf8\xf8\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8"
            "\xf8\xf8\xf8\xf8\xf8\xf8\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8"
            "\xf8\xf8\xf8\xf8\xf8\xf8\xf8\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8"
            "\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\x00\x00\x00\x00\x00\x00\x00"
            "\x00\x00\x00\x00\x00\x00\x00\x00\x00\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\x00\x00\x00\x00\x00\x00"
            "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\x00\x00\x00\x00\x00"
            "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\x00\x00\x00\x00"
            "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\x00\x00\x00"
            "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\x00\x00"
            "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\x00"
            "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8\xf8"
            "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";

        constexpr std::string_view fileTypeToString(Filesystem::FileType type)
        {
            switch (type) {
                case Filesystem::FileType::Shader:
                    return "Shader";
                case Filesystem::FileType::Model:
                    return "Model";
                case Filesystem::FileType::RawImage:
                case Filesystem::FileType::BasisImage:
                    return "Image";
                case Filesystem::FileType::WAV:
                case Filesystem::FileType::OGG:
                    return "Audio";
                case Filesystem::FileType::RawBytes:
                default:
                    return "Raw";
            }
        }

    } // namespace detail

    AssetManager::AssetManager(const graphics::DevicePtr& device) : device_ { device }
    {
        createMissingTexture();

        selectOneChannel();
        selectTwoChannels();
        selectThreeChannels();
        selectFourChannels();
    }

    AssetManagerPtr AssetManager::create(const graphics::DevicePtr& device)
    {
        COFFEE_ASSERT(device != nullptr, "Invalid device provided.");

        return std::shared_ptr<AssetManager>(new AssetManager { device });
    }

    std::vector<uint8_t> AssetManager::loadBytes(const BytesLoadingInfo& loadingInfo)
    {
        HashAccessor accessor {};
        XXH64_hash_t hash = XXH3_64bits(loadingInfo.path.data(), loadingInfo.path.size());

        if (!cache_.find(accessor, hash)) {
            if (loadingInfo.filesystem == nullptr) {
                throw AssetException { AssetException::Type::NotInCache,
                                       fmt::format("Requested asset '{}' wasn't in cache, and filesystem wasn't provided", loadingInfo.path) };
            }

            Filesystem::Entry entry = loadingInfo.filesystem->getMetadata(loadingInfo.path);

            if (entry.type != Filesystem::FileType::RawBytes) {
                throw AssetException { AssetException::Type::TypeMismatch,
                                       fmt::format("Expected type Raw, requested type was {}", detail::fileTypeToString(entry.type)) };
            }

            std::vector<uint8_t> rawBytes = loadingInfo.filesystem->getContent(loadingInfo.path);

            cache_.insert(std::make_pair(hash, Asset::create(std::make_shared<std::vector<uint8_t>>(rawBytes))));
        }

        auto& asset = accessor->second;

        if (asset.type != Filesystem::FileType::RawBytes) {
            throw AssetException { AssetException::Type::TypeMismatch,
                                   fmt::format("Expected type Raw, requested type was {}", detail::fileTypeToString(asset.type)) };
        }

        return *std::static_pointer_cast<std::vector<uint8_t>>(asset.actualObject);
    }

    graphics::ShaderPtr AssetManager::loadShader(const ShaderLoadingInfo& loadingInfo)
    {
        HashAccessor accessor {};
        XXH64_hash_t hash = XXH3_64bits(loadingInfo.path.data(), loadingInfo.path.size());

        if (!cache_.find(accessor, hash)) {
            if (loadingInfo.filesystem == nullptr) {
                throw AssetException { AssetException::Type::NotInCache,
                                       fmt::format("Requested asset '{}' wasn't in cache, and filesystem wasn't provided", loadingInfo.path) };
            }

            Filesystem::Entry entry = loadingInfo.filesystem->getMetadata(loadingInfo.path);

            if (entry.type != Filesystem::FileType::Shader) {
                throw AssetException { AssetException::Type::TypeMismatch,
                                       fmt::format("Expected type Shader, requested type was {}", detail::fileTypeToString(entry.type)) };
            }

            graphics::ShaderPtr shader =
                graphics::ShaderModule::create(device_, loadingInfo.filesystem->getContent(loadingInfo.path), loadingInfo.entrypoint);
            cache_.insert(std::make_pair(hash, Asset::create(shader)));

            return shader;
        }

        auto& asset = accessor->second;

        if (asset.type != Filesystem::FileType::Shader) {
            throw AssetException { AssetException::Type::TypeMismatch,
                                   fmt::format("Expected type Shader, requested type was {}", detail::fileTypeToString(asset.type)) };
        }

        return std::static_pointer_cast<graphics::ShaderModule>(asset.actualObject);
    }

    graphics::ImagePtr AssetManager::loadImage(const ImageLoadingInfo& loadingInfo)
    {
        HashAccessor accessor {};
        XXH64_hash_t hash = XXH3_64bits(loadingInfo.path.data(), loadingInfo.path.size());

        if (!cache_.find(accessor, hash)) {
            if (loadingInfo.filesystem == nullptr) {
                throw AssetException { AssetException::Type::NotInCache,
                                       fmt::format("Requested asset '{}' wasn't in cache, and filesystem wasn't provided", loadingInfo.path) };
            }

            Filesystem::Entry entry = loadingInfo.filesystem->getMetadata(loadingInfo.path);

            if (entry.type != Filesystem::FileType::RawImage && entry.type != Filesystem::FileType::BasisImage) {
                throw AssetException { AssetException::Type::TypeMismatch,
                                       fmt::format("Expected type Image, requested type was {}", detail::fileTypeToString(entry.type)) };
            }

            graphics::ImagePtr image = nullptr;
            std::vector<uint8_t> rawBytes = loadingInfo.filesystem->getContent(loadingInfo.path);

            switch (entry.type) {
                case Filesystem::FileType::RawImage:
                    image = loadRawImage(rawBytes);
                    break;
                case Filesystem::FileType::BasisImage:
                    image = loadBasisImage(rawBytes);
                    break;
                default:
                    COFFEE_ASSERT(false, "Should not happen.");
                    break;
            }

            cache_.insert(std::make_pair(hash, Asset::create(image)));

            return image;
        }

        auto& asset = accessor->second;

        if (asset.type != Filesystem::FileType::RawImage) {
            throw AssetException { AssetException::Type::TypeMismatch,
                                   fmt::format("Expected type Image, requested type was {}", detail::fileTypeToString(asset.type)) };
        }

        return std::static_pointer_cast<graphics::Image>(asset.actualObject);
    }

    graphics::ModelPtr AssetManager::loadModel(const ModelLoadingInfo& loadingInfo)
    {
        HashAccessor accessor {};
        XXH64_hash_t hash = XXH3_64bits(loadingInfo.path.data(), loadingInfo.path.size());

        if (!cache_.find(accessor, hash)) {
            if (loadingInfo.filesystem == nullptr) {
                throw AssetException { AssetException::Type::NotInCache,
                                       fmt::format("Requested asset '{}' wasn't in cache, and filesystem wasn't provided", loadingInfo.path) };
            }

            Filesystem::Entry entry = loadingInfo.filesystem->getMetadata(loadingInfo.path);

            if (entry.type != Filesystem::FileType::Model) {
                throw AssetException { AssetException::Type::TypeMismatch,
                                       fmt::format("Expected type Model, requested type was {}", detail::fileTypeToString(entry.type)) };
            }

            graphics::ModelPtr model = loadModel(loadingInfo.filesystem, loadingInfo.path);
            cache_.insert(std::make_pair(hash, Asset::create(model)));

            return model;
        }

        auto& asset = accessor->second;

        if (asset.type != Filesystem::FileType::Model) {
            throw AssetException { AssetException::Type::TypeMismatch,
                                   fmt::format("Expected type Model, requested type was {}", detail::fileTypeToString(asset.type)) };
        }

        return std::static_pointer_cast<graphics::Model>(asset.actualObject);
    }

    void AssetManager::removeFromCache(const std::string& path) { cache_.erase(XXH3_64bits(path.data(), path.size())); }

    void AssetManager::createMissingTexture()
    {
        using namespace graphics;

        ImageConfiguration imageConfiguration {};
        imageConfiguration.imageType = VK_IMAGE_TYPE_2D;
        imageConfiguration.format = VK_FORMAT_R8G8_UNORM;
        imageConfiguration.extent = { 16U, 16U, 1U };
        imageConfiguration.mipLevels = 1U;
        imageConfiguration.arrayLayers = 1U;
        imageConfiguration.samples = VK_SAMPLE_COUNT_1_BIT;
        imageConfiguration.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageConfiguration.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageConfiguration.priority = 1.0f;
        missingImage_ = Image::create(device_, imageConfiguration);

        BufferConfiguration stagingBufferConfiguration {};
        stagingBufferConfiguration.instanceSize = 2U;
        stagingBufferConfiguration.instanceCount = 16U * 16U;
        stagingBufferConfiguration.usageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        stagingBufferConfiguration.memoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        stagingBufferConfiguration.allocationFlags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        stagingBufferConfiguration.allocationUsage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
        auto stagingBuffer = Buffer::create(device_, stagingBufferConfiguration);

        std::memcpy(stagingBuffer->memory(), detail::kMissingTextureBytes, sizeof(detail::kMissingTextureBytes) - 1U);
        stagingBuffer->flush();

        CommandBuffer transferCommandBuffer = CommandBuffer::createTransfer(device_);
        VkImageMemoryBarrier barrier { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        VkBufferImageCopy copyRegion {};

        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = missingImage_->image();
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = 1;
        transferCommandBuffer.imagePipelineBarrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &barrier);

        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageExtent.width = missingImage_->extent.width;
        copyRegion.imageExtent.height = missingImage_->extent.height;
        copyRegion.imageExtent.depth = missingImage_->extent.depth;
        transferCommandBuffer.copyBufferToImage(stagingBuffer, missingImage_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = device_->isUnifiedGraphicsTransferQueue() ? VK_ACCESS_SHADER_READ_BIT : static_cast<VkAccessFlags>(0);
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcQueueFamilyIndex = device_->transferQueueFamilyIndex();
        barrier.dstQueueFamilyIndex = device_->graphicsQueueFamilyIndex();
        barrier.image = missingImage_->image();
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = 1;
        VkPipelineStageFlagBits useStage =
            device_->isUnifiedGraphicsTransferQueue() ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

        transferCommandBuffer.imagePipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, useStage, 0, 1, &barrier);
        device_->submit(std::move(transferCommandBuffer), {}, nullptr, true);

        if (!device_->isUnifiedGraphicsTransferQueue()) {
            CommandBuffer ownershipCommandBuffer = CommandBuffer::createGraphics(device_);

            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcQueueFamilyIndex = device_->transferQueueFamilyIndex();
            barrier.dstQueueFamilyIndex = device_->graphicsQueueFamilyIndex();
            barrier.image = missingImage_->image();
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.layerCount = 1;

            ownershipCommandBuffer.imagePipelineBarrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 1, &barrier);
            device_->submit(std::move(ownershipCommandBuffer), {}, nullptr, true);
        }

        ImageViewConfiguration viewConfiguration {};
        viewConfiguration.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewConfiguration.format = VK_FORMAT_R8G8_UNORM;
        viewConfiguration.components.r = VK_COMPONENT_SWIZZLE_R;
        viewConfiguration.components.g = VK_COMPONENT_SWIZZLE_ZERO;
        viewConfiguration.components.b = VK_COMPONENT_SWIZZLE_G;
        viewConfiguration.components.a = VK_COMPONENT_SWIZZLE_ONE;
        missingTexture_ = ImageView::create(missingImage_, viewConfiguration);
    }

    void AssetManager::selectOneChannel()
    {
        VkFormatProperties formatProperties;

        vkGetPhysicalDeviceFormatProperties(device_->physicalDevice(), VK_FORMAT_BC4_UNORM_BLOCK, &formatProperties);
        if (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) {
            compressionTypes_.vkOneChannel = VK_FORMAT_BC4_UNORM_BLOCK;
            compressionTypes_.basisOneChannel = basist::transcoder_texture_format::cTFBC4_R;
            return;
        }

        vkGetPhysicalDeviceFormatProperties(device_->physicalDevice(), VK_FORMAT_EAC_R11_UNORM_BLOCK, &formatProperties);
        if (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) {
            compressionTypes_.vkOneChannel = VK_FORMAT_EAC_R11_UNORM_BLOCK;
            compressionTypes_.basisOneChannel = basist::transcoder_texture_format::cTFETC2_EAC_R11;
            return;
        }

        vkGetPhysicalDeviceFormatProperties(device_->physicalDevice(), VK_FORMAT_ASTC_4x4_UNORM_BLOCK, &formatProperties);
        if (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) {
            compressionTypes_.vkOneChannel = VK_FORMAT_ASTC_4x4_UNORM_BLOCK;
            compressionTypes_.basisOneChannel = basist::transcoder_texture_format::cTFASTC_4x4_RGBA;
            return;
        }

        // TODO: Check whether this will ever work properly
        compressionTypes_.vkOneChannel = VK_FORMAT_R8_UNORM;
        compressionTypes_.basisOneChannel = basist::transcoder_texture_format::cTFRGBA32;
    }

    void AssetManager::selectTwoChannels()
    {
        VkFormatProperties formatProperties;

        vkGetPhysicalDeviceFormatProperties(device_->physicalDevice(), VK_FORMAT_BC5_UNORM_BLOCK, &formatProperties);
        if (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) {
            compressionTypes_.vkTwoChannels = VK_FORMAT_BC5_UNORM_BLOCK;
            compressionTypes_.basisTwoChannels = basist::transcoder_texture_format::cTFBC5_RG;
            return;
        }

        vkGetPhysicalDeviceFormatProperties(device_->physicalDevice(), VK_FORMAT_EAC_R11G11_UNORM_BLOCK, &formatProperties);
        if (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) {
            compressionTypes_.vkTwoChannels = VK_FORMAT_EAC_R11G11_UNORM_BLOCK;
            compressionTypes_.basisTwoChannels = basist::transcoder_texture_format::cTFETC2_EAC_RG11;
            return;
        }

        vkGetPhysicalDeviceFormatProperties(device_->physicalDevice(), VK_FORMAT_ASTC_4x4_UNORM_BLOCK, &formatProperties);
        if (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) {
            compressionTypes_.vkTwoChannels = VK_FORMAT_ASTC_4x4_UNORM_BLOCK;
            compressionTypes_.basisTwoChannels = basist::transcoder_texture_format::cTFASTC_4x4_RGBA;
            return;
        }

        // TODO: Check whether this will ever work properly
        compressionTypes_.vkTwoChannels = VK_FORMAT_R8G8_UNORM;
        compressionTypes_.basisTwoChannels = basist::transcoder_texture_format::cTFRGBA32;
    }

    void AssetManager::selectThreeChannels()
    {
        VkFormatProperties formatProperties;

        vkGetPhysicalDeviceFormatProperties(device_->physicalDevice(), VK_FORMAT_BC1_RGB_UNORM_BLOCK, &formatProperties);
        if (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) {
            compressionTypes_.vkThreeChannels = VK_FORMAT_BC1_RGB_UNORM_BLOCK;
            compressionTypes_.basisThreeChannels = basist::transcoder_texture_format::cTFBC1_RGB;
            return;
        }

        vkGetPhysicalDeviceFormatProperties(device_->physicalDevice(), VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK, &formatProperties);
        if (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) {
            compressionTypes_.vkThreeChannels = VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK;
            compressionTypes_.basisThreeChannels = basist::transcoder_texture_format::cTFETC1_RGB;
            return;
        }

        vkGetPhysicalDeviceFormatProperties(device_->physicalDevice(), VK_FORMAT_ASTC_4x4_UNORM_BLOCK, &formatProperties);
        if (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) {
            compressionTypes_.vkThreeChannels = VK_FORMAT_ASTC_4x4_UNORM_BLOCK;
            compressionTypes_.basisThreeChannels = basist::transcoder_texture_format::cTFASTC_4x4_RGBA;
            return;
        }

        // This must work properly because most GPU's doesn't support R8G8B8 (24 bits) because of misalignment
        compressionTypes_.vkThreeChannels = VK_FORMAT_R8G8B8A8_UNORM;
        compressionTypes_.basisThreeChannels = basist::transcoder_texture_format::cTFRGBA32;
    }

    void AssetManager::selectFourChannels()
    {
        VkFormatProperties formatProperties;

        vkGetPhysicalDeviceFormatProperties(device_->physicalDevice(), VK_FORMAT_BC7_UNORM_BLOCK, &formatProperties);
        if (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) {
            compressionTypes_.vkFourChannels = VK_FORMAT_BC7_UNORM_BLOCK;
            compressionTypes_.basisFourChannels = basist::transcoder_texture_format::cTFBC7_RGBA;
            return;
        }

        vkGetPhysicalDeviceFormatProperties(device_->physicalDevice(), VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK, &formatProperties);
        if (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) {
            compressionTypes_.vkFourChannels = VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;
            compressionTypes_.basisFourChannels = basist::transcoder_texture_format::cTFETC2_RGBA;
            return;
        }

        vkGetPhysicalDeviceFormatProperties(device_->physicalDevice(), VK_FORMAT_ASTC_4x4_UNORM_BLOCK, &formatProperties);
        if (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) {
            compressionTypes_.vkFourChannels = VK_FORMAT_ASTC_4x4_UNORM_BLOCK;
            compressionTypes_.basisFourChannels = basist::transcoder_texture_format::cTFASTC_4x4_RGBA;
            return;
        }

        compressionTypes_.vkFourChannels = VK_FORMAT_R8G8B8A8_UNORM;
        compressionTypes_.basisThreeChannels = basist::transcoder_texture_format::cTFRGBA32;
    }

    graphics::ModelPtr AssetManager::loadModel(const FilesystemPtr& filesystem, const std::string& path)
    {
        using namespace graphics;

        constexpr uint8_t headerMagic[4] = { 0xF0, 0x7B, 0xAE, 0x31 };
        constexpr uint8_t meshMagic[4] = { 0x13, 0xEA, 0xB7, 0xF0 };

        std::vector<uint8_t> modelStream = filesystem->getContent(path);
        utils::ReaderStream stream { modelStream };

        if (stream.size() < 8) {
            throw FilesystemException { FilesystemException::Type::InvalidFileType, "Invalid header size!" };
        }

        if (std::memcmp(stream.readBuffer<uint8_t, 4>(), headerMagic, 4) != 0) {
            throw FilesystemException { FilesystemException::Type::InvalidFileType, "Invalid header magic!" };
        }

        uint32_t meshesSize = stream.read<uint32_t>();

        std::vector<Vertex> vertices {};
        std::vector<uint32_t> indices {};
        std::vector<MaterialMetadata> materialsMetadata {};
        std::vector<MaterialMetadata> uploadsMetadata {};
        std::vector<MeshMetadata> meshesMetadata {};
        std::vector<Materials> materials {};

        materialsMetadata.reserve(7ULL * meshesSize);
        meshesMetadata.reserve(meshesSize);
        materials.reserve(meshesSize);

        for (uint32_t i = 0; i < meshesSize; i++) {
            if (std::memcmp(stream.readBuffer<uint8_t, 4>(), meshMagic, 4) != 0) {
                throw FilesystemException { FilesystemException::Type::InvalidFileType, "Invalid mesh magic!" };
            }

            uint32_t verticesSize = stream.read<uint32_t>();
            uint32_t indicesSize = stream.read<uint32_t>();

            glm::vec3 aabbMin {};
            glm::vec3 aabbMax {};
            stream.readDirectly(&aabbMin);
            stream.readDirectly(&aabbMax);

            materials.push_back({ missingTexture_ });
            stream.readDirectly(&materials[i].modifiers.diffuseColor);
            stream.readDirectly(&materials[i].modifiers.specularColor);
            stream.readDirectly(&materials[i].modifiers.metallicFactor);
            stream.readDirectly(&materials[i].modifiers.roughnessFactor);

            materialsMetadata.push_back({ &materials[i], readMaterialName(stream), TextureType::Diffuse });
            materialsMetadata.push_back({ &materials[i], readMaterialName(stream), TextureType::Specular });
            materialsMetadata.push_back({ &materials[i], readMaterialName(stream), TextureType::Normals });
            materialsMetadata.push_back({ &materials[i], readMaterialName(stream), TextureType::Emissive });
            materialsMetadata.push_back({ &materials[i], readMaterialName(stream), TextureType::Roughness });
            materialsMetadata.push_back({ &materials[i], readMaterialName(stream), TextureType::Metallic });
            materialsMetadata.push_back({ &materials[i], readMaterialName(stream), TextureType::AmbientOcclusion });

            uint32_t verticesOffset = static_cast<uint32_t>(vertices.size());
            uint32_t indicesOffset = static_cast<uint32_t>(indices.size());

            vertices.resize(vertices.size() + verticesSize);
            indices.resize(indices.size() + indicesSize);
            stream.readDirectly(vertices.data() + verticesOffset, verticesSize);
            stream.readDirectly(indices.data() + indicesOffset, indicesSize);

            AABB aabb {};
            aabb.min = glm::vec4 { aabbMin, 1.0f };
            aabb.max = glm::vec4 { aabbMax, 1.0f };

            meshesMetadata.push_back({ std::move(aabb), verticesOffset, verticesSize, indicesOffset, indicesSize });
        }

        size_t instanceCount = vertices.size() * sizeof(Vertex) + indices.size() * sizeof(uint32_t);
        constexpr size_t instanceSize = sizeof(uint32_t);

        BufferConfiguration stagingBufferConfiguration {};
        stagingBufferConfiguration.instanceSize = instanceSize;
        stagingBufferConfiguration.instanceCount = instanceCount / instanceSize;
        stagingBufferConfiguration.usageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        stagingBufferConfiguration.memoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        stagingBufferConfiguration.allocationFlags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        stagingBufferConfiguration.allocationUsage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
        auto stagingBuffer = Buffer::create(device_, stagingBufferConfiguration);

        BufferConfiguration verticesBufferConfiguration {};
        verticesBufferConfiguration.instanceSize = sizeof(Vertex);
        verticesBufferConfiguration.instanceCount = vertices.size();
        verticesBufferConfiguration.usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        verticesBufferConfiguration.memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        verticesBufferConfiguration.allocationUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        auto verticesBuffer = Buffer::create(device_, verticesBufferConfiguration);

        BufferConfiguration indicesBufferConfiguration {};
        indicesBufferConfiguration.instanceSize = sizeof(uint32_t);
        indicesBufferConfiguration.instanceCount = indices.size();
        indicesBufferConfiguration.usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        indicesBufferConfiguration.memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        indicesBufferConfiguration.allocationUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        auto indicesBuffer = Buffer::create(device_, indicesBufferConfiguration);

        uint8_t* memory = static_cast<uint8_t*>(stagingBuffer->memory());
        std::memcpy(memory, vertices.data(), vertices.size() * sizeof(Vertex));
        std::memcpy(memory + vertices.size() * sizeof(Vertex), indices.data(), indices.size() * sizeof(uint32_t));
        stagingBuffer->flush();

        CommandBuffer transferCommandBuffer = CommandBuffer::createTransfer(device_);

        VkBufferCopy verticesCopyRegion {};
        verticesCopyRegion.srcOffset = 0;
        verticesCopyRegion.size = vertices.size() * sizeof(Vertex);
        transferCommandBuffer.copyBuffer(stagingBuffer, verticesBuffer, 1, &verticesCopyRegion);

        VkBufferCopy indicesCopyRegion {};
        indicesCopyRegion.srcOffset = vertices.size() * sizeof(Vertex);
        indicesCopyRegion.size = indices.size() * sizeof(uint32_t);
        transferCommandBuffer.copyBuffer(stagingBuffer, indicesBuffer, 1, &indicesCopyRegion);

        device_->submit(std::move(transferCommandBuffer), {}, nullptr, true);

        for (size_t index = 0; index < materialsMetadata.size(); index++) {
            auto& metadata = materialsMetadata[index];

            if (metadata.name.empty()) {
                continue;
            }

            HashAccessor accessor {};
            XXH64_hash_t hash = XXH3_64bits(metadata.name.data(), metadata.name.size());

            if (cache_.find(accessor, hash)) {
                auto& asset = accessor->second;

                if (asset.type != Filesystem::FileType::RawImage) {
                    throw AssetException { AssetException::Type::TypeMismatch,
                                           fmt::format("Expected type Image, requested type was {}", detail::fileTypeToString(asset.type)) };
                }

                auto image = std::static_pointer_cast<graphics::Image>(asset.actualObject);

                graphics::ImageViewConfiguration viewConfiguration {};
                viewConfiguration.viewType = VK_IMAGE_VIEW_TYPE_2D;
                viewConfiguration.format = image->imageFormat;
                viewConfiguration.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                viewConfiguration.subresourceRange.baseMipLevel = 0;
                viewConfiguration.subresourceRange.levelCount = image->mipLevels;
                viewConfiguration.subresourceRange.baseArrayLayer = 0;
                viewConfiguration.subresourceRange.layerCount = image->arrayLayers;

                metadata.materials->write(graphics::ImageView::create(image, viewConfiguration), metadata.type);
                continue;
            }

            Filesystem::Entry entry = filesystem->getMetadata(metadata.name);
            std::vector<uint8_t> rawBytes = filesystem->getContent(metadata.name);
            graphics::ImagePtr image = nullptr;

            switch (entry.type) {
                case Filesystem::FileType::RawImage:
                    image = loadRawImage(rawBytes);
                    break;
                case Filesystem::FileType::BasisImage:
                    image = loadBasisImage(rawBytes);
                    break;
                default:
                    COFFEE_ASSERT(false, "Should not happen.");
                    break;
            }

            graphics::ImageViewConfiguration viewConfiguration {};
            viewConfiguration.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewConfiguration.format = image->imageFormat;
            viewConfiguration.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewConfiguration.subresourceRange.baseMipLevel = 0;
            viewConfiguration.subresourceRange.levelCount = image->mipLevels;
            viewConfiguration.subresourceRange.baseArrayLayer = 0;
            viewConfiguration.subresourceRange.layerCount = image->arrayLayers;

            metadata.materials->write(graphics::ImageView::create(image, viewConfiguration), metadata.type);
            cache_.insert(std::make_pair(hash, Asset::create(image)));
        }

        std::vector<Mesh> meshes {};
        meshes.reserve(meshesSize);

        for (uint32_t i = 0; i < meshesSize; i++) {
            meshes.emplace_back(
                std::move(materials[i]),
                std::move(meshesMetadata[i].aabb),
                meshesMetadata[i].verticesOffset,
                meshesMetadata[i].indicesOffset,
                meshesMetadata[i].verticesSize,
                meshesMetadata[i].indicesSize
            );
        }

        return std::make_shared<Model>(std::move(meshes), std::move(verticesBuffer), std::move(indicesBuffer));
    }

    std::string AssetManager::readMaterialName(utils::ReaderStream& stream)
    {
        uint8_t size = stream.read<uint8_t>();

        if (size == 0) {
            return {};
        }

        std::string outputString {};
        outputString.resize(size);

        stream.readDirectly(outputString.data(), size);

        return outputString;
    }

    graphics::ImagePtr AssetManager::loadRawImage(std::vector<uint8_t>& rawBytes)
    {
        using namespace graphics;

        utils::ReadOnlyStream<4> stream { rawBytes };
        uint32_t width = stream.read<uint32_t>();
        uint32_t height = stream.read<uint32_t>();
        uint32_t amountOfChannels = stream.read<uint32_t>();

        ImageConfiguration imageConfiguration {};
        imageConfiguration.imageType = VK_IMAGE_TYPE_2D;
        imageConfiguration.format = channelsToVkFormat(amountOfChannels, false);
        imageConfiguration.extent = { width, height, 1U };
        imageConfiguration.mipLevels = 1U;
        imageConfiguration.arrayLayers = 1U;
        imageConfiguration.samples = VK_SAMPLE_COUNT_1_BIT;
        imageConfiguration.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageConfiguration.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        auto image = Image::create(device_, imageConfiguration);

        BufferConfiguration stagingBufferConfiguration {};
        stagingBufferConfiguration.instanceSize = amountOfChannels;
        stagingBufferConfiguration.instanceCount = width * height;
        stagingBufferConfiguration.usageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        stagingBufferConfiguration.memoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        stagingBufferConfiguration.allocationFlags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        stagingBufferConfiguration.allocationUsage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
        auto stagingBuffer = Buffer::create(device_, stagingBufferConfiguration);

        std::memcpy(stagingBuffer->memory(), rawBytes.data() + stream.offset(), rawBytes.size() - stream.offset());
        stagingBuffer->flush(rawBytes.size() - stream.offset());

        CommandBuffer transferCommandBuffer = CommandBuffer::createTransfer(device_);
        VkImageMemoryBarrier barrier { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        VkBufferImageCopy copyRegion {};

        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image->image();
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = 1;
        transferCommandBuffer.imagePipelineBarrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &barrier);

        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageExtent.width = image->extent.width;
        copyRegion.imageExtent.height = image->extent.height;
        copyRegion.imageExtent.depth = image->extent.depth;
        transferCommandBuffer.copyBufferToImage(stagingBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = device_->isUnifiedGraphicsTransferQueue() ? VK_ACCESS_SHADER_READ_BIT : static_cast<VkAccessFlags>(0);
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcQueueFamilyIndex = device_->transferQueueFamilyIndex();
        barrier.dstQueueFamilyIndex = device_->graphicsQueueFamilyIndex();
        barrier.image = image->image();
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = 1;
        VkPipelineStageFlagBits useStage =
            device_->isUnifiedGraphicsTransferQueue() ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

        transferCommandBuffer.imagePipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, useStage, 0, 1, &barrier);
        device_->submit(std::move(transferCommandBuffer), {}, nullptr, true);

        if (!device_->isUnifiedGraphicsTransferQueue()) {
            CommandBuffer ownershipCommandBuffer = CommandBuffer::createGraphics(device_);

            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcQueueFamilyIndex = device_->transferQueueFamilyIndex();
            barrier.dstQueueFamilyIndex = device_->graphicsQueueFamilyIndex();
            barrier.image = image->image();
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.layerCount = 1;

            ownershipCommandBuffer.imagePipelineBarrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 1, &barrier);
            device_->submit(std::move(ownershipCommandBuffer), {}, nullptr, true);
        }

        return image;
    }

    graphics::ImagePtr AssetManager::loadBasisImage(std::vector<uint8_t>& rawBytes)
    {
        using namespace graphics;

        basist::ktx2_transcoder transcoder {};

        if (!transcoder.init(rawBytes.data(), rawBytes.size())) {
            throw AssetException { AssetException::Type::ImplementationFailure, "Failed to begin transcoding process!" };
        }

        auto* amountOfChannelsInImage = transcoder.find_key(detail::kBasisChannelCountField);

        if (amountOfChannelsInImage == nullptr || amountOfChannelsInImage->size() != sizeof(uint32_t)) {
            throw AssetException { AssetException::Type::TypeMismatch, "Basis image doesn't contain required fields!" };
        }

        uint32_t amountOfChannels = 0;
        std::memcpy(&amountOfChannels, amountOfChannelsInImage->data(), sizeof(uint32_t));

        if (!transcoder.start_transcoding()) {
            throw AssetException { AssetException::Type::ImplementationFailure, "Failed to begin transcoding process!" };
        }

        const basist::transcoder_texture_format format = channelsToBasisuFormat(amountOfChannels);
        const uint32_t bytesPerBlock = basist::basis_get_bytes_per_block_or_pixel(format);
        const bool isBlockFormat = !basist::basis_transcoder_format_is_uncompressed(format);
        basist::ktx2_image_level_info levelInfo {};
        size_t allocationSize = 0ULL;

        for (uint32_t faceIndex = 0; faceIndex < transcoder.get_faces(); faceIndex++) {
            for (uint32_t mipmapLevel = 0; mipmapLevel < transcoder.get_levels(); mipmapLevel++) {
                transcoder.get_image_level_info(levelInfo, mipmapLevel, 0, faceIndex);

                if (levelInfo.m_width < 64 && levelInfo.m_height < 64) {
                    continue;
                }

                allocationSize += isBlockFormat ? static_cast<size_t>(levelInfo.m_num_blocks_x) * levelInfo.m_num_blocks_y * bytesPerBlock
                                                : static_cast<size_t>(levelInfo.m_width) * levelInfo.m_height;
            }
        }

        std::unique_ptr<uint8_t[]> imageBuffer = std::make_unique<uint8_t[]>(allocationSize);
        std::vector<std::vector<MipmapInformation>> mipmapInformations {};
        uint32_t width = 0;
        uint32_t height = 0;
        size_t offset = 0ULL;

        for (uint32_t faceIndex = 0; faceIndex < transcoder.get_faces(); faceIndex++) {
            mipmapInformations.emplace_back();

            for (uint32_t mipmapLevel = 0; mipmapLevel < transcoder.get_levels(); mipmapLevel++) {
                transcoder.get_image_level_info(levelInfo, mipmapLevel, 0, faceIndex);

                if (levelInfo.m_width < 64 && levelInfo.m_height < 64) {
                    continue;
                }

                uint32_t mipmapSize =
                    isBlockFormat ? levelInfo.m_num_blocks_x * levelInfo.m_num_blocks_y * bytesPerBlock : levelInfo.m_width * levelInfo.m_height;
                transcoder.transcode_image_level(mipmapLevel, 0, faceIndex, imageBuffer.get() + offset, mipmapSize, format);

                MipmapInformation info {};
                info.bufferOffset = offset;
                info.width = levelInfo.m_width;
                info.height = levelInfo.m_height;
                mipmapInformations.back().push_back(std::move(info));

                offset += mipmapSize;
                width = std::max(width, levelInfo.m_width);
                height = std::max(height, levelInfo.m_height);
            }
        }

        size_t instanceSize = 2;
        while (allocationSize % instanceSize != 0) {
            instanceSize++;
        }

        BufferConfiguration stagingBufferConfiguration {};
        stagingBufferConfiguration.instanceSize = instanceSize;
        stagingBufferConfiguration.instanceCount = allocationSize / instanceSize;
        stagingBufferConfiguration.usageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        stagingBufferConfiguration.memoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        stagingBufferConfiguration.allocationFlags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        stagingBufferConfiguration.allocationUsage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
        auto stagingBuffer = Buffer::create(device_, stagingBufferConfiguration);

        std::memcpy(stagingBuffer->memory(), imageBuffer.get(), allocationSize);
        stagingBuffer->flush();

        ImageConfiguration imageConfiguration {};
        imageConfiguration.imageType = VK_IMAGE_TYPE_2D;
        imageConfiguration.format = channelsToVkFormat(amountOfChannels, true);
        imageConfiguration.extent = { width, height, 1U };
        imageConfiguration.mipLevels = static_cast<uint32_t>(mipmapInformations[0].size());
        imageConfiguration.arrayLayers = static_cast<uint32_t>(mipmapInformations.size());
        imageConfiguration.samples = VK_SAMPLE_COUNT_1_BIT;
        imageConfiguration.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageConfiguration.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

        if (imageConfiguration.arrayLayers == 6) {
            imageConfiguration.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        }

        auto image = Image::create(device_, imageConfiguration);

        std::vector<VkBufferImageCopy> copyRegions {};
        VkImageMemoryBarrier barrier { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };

        CommandBuffer transferCommandBuffer = CommandBuffer::createTransfer(device_);
        copyRegions.reserve(mipmapInformations[0].size() * mipmapInformations.size());

        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image->image();
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.levelCount = static_cast<uint32_t>(mipmapInformations[0].size());
        barrier.subresourceRange.layerCount = static_cast<uint32_t>(mipmapInformations.size());
        transferCommandBuffer.imagePipelineBarrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &barrier);

        for (size_t faceIndex = 0; faceIndex < mipmapInformations.size(); faceIndex++) {
            auto& face = mipmapInformations[faceIndex];

            for (size_t mipmapLevel = 0; mipmapLevel < face.size(); mipmapLevel++) {
                auto& mipmap = face[mipmapLevel];

                VkBufferImageCopy copyRegion {};
                copyRegion.bufferOffset = mipmap.bufferOffset;
                copyRegion.bufferRowLength = 0;
                copyRegion.bufferImageHeight = 0;
                copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                copyRegion.imageSubresource.mipLevel = mipmapLevel;
                copyRegion.imageSubresource.baseArrayLayer = faceIndex;
                copyRegion.imageSubresource.layerCount = 1;
                copyRegion.imageOffset = { 0, 0, 0 };
                copyRegion.imageExtent = { mipmap.width, mipmap.height, 1U };

                copyRegions.push_back(std::move(copyRegion));
            }
        }

        transferCommandBuffer.copyBufferToImage(stagingBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, copyRegions.size(), copyRegions.data());

        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = device_->isUnifiedGraphicsTransferQueue() ? VK_ACCESS_SHADER_READ_BIT : static_cast<VkAccessFlags>(0);
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcQueueFamilyIndex = device_->transferQueueFamilyIndex();
        barrier.dstQueueFamilyIndex = device_->graphicsQueueFamilyIndex();
        barrier.image = image->image();
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.levelCount = static_cast<uint32_t>(mipmapInformations[0].size());
        barrier.subresourceRange.layerCount = static_cast<uint32_t>(mipmapInformations.size());
        VkPipelineStageFlagBits useStage =
            device_->isUnifiedGraphicsTransferQueue() ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

        transferCommandBuffer.imagePipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, useStage, 0, 1, &barrier);
        device_->submit(std::move(transferCommandBuffer), {}, nullptr, true);

        if (!device_->isUnifiedGraphicsTransferQueue()) {
            CommandBuffer ownershipCommandBuffer = CommandBuffer::createGraphics(device_);

            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcQueueFamilyIndex = device_->transferQueueFamilyIndex();
            barrier.dstQueueFamilyIndex = device_->graphicsQueueFamilyIndex();
            barrier.image = image->image();
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.levelCount = static_cast<uint32_t>(mipmapInformations[0].size());
            barrier.subresourceRange.layerCount = static_cast<uint32_t>(mipmapInformations.size());

            ownershipCommandBuffer.imagePipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 1, &barrier);
            device_->submit(std::move(ownershipCommandBuffer), {}, nullptr, true);
        }

        return image;
    }

    VkFormat AssetManager::channelsToVkFormat(uint32_t amountOfChannels, bool compressed)
    {
        switch (amountOfChannels) {
            case 1:
                return compressed ? compressionTypes_.vkOneChannel : VK_FORMAT_R8_UNORM;
            case 2:
                return compressed ? compressionTypes_.vkTwoChannels : VK_FORMAT_R8G8_UNORM;
            case 3:
                return compressed ? compressionTypes_.vkThreeChannels : VK_FORMAT_R8G8B8A8_UNORM;
            case 4:
                return compressed ? compressionTypes_.vkFourChannels : VK_FORMAT_R8G8B8A8_UNORM;
        }

        COFFEE_ASSERT(false, "Invalid channel count provided.");
        return VK_FORMAT_UNDEFINED;
    }

    basist::transcoder_texture_format AssetManager::channelsToBasisuFormat(uint32_t amountOfChannels)
    {
        switch (amountOfChannels) {
            case 1:
                return compressionTypes_.basisOneChannel;
            case 2:
                return compressionTypes_.basisTwoChannels;
            case 3:
                return compressionTypes_.basisThreeChannels;
            case 4:
                return compressionTypes_.basisFourChannels;
        }

        COFFEE_ASSERT(false, "Invalid channel count provided.");
        return basist::transcoder_texture_format::cTFTotalTextureFormats;
    }

} // namespace coffee
