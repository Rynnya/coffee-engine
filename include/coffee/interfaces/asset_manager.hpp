#pragma once

#include <coffee/graphics/buffer.hpp>
#include <coffee/graphics/image.hpp>
#include <coffee/graphics/shader.hpp>
#include <coffee/objects/model.hpp>

#include <coffee/interfaces/filesystem.hpp>
#include <coffee/utils/utils.hpp>

#include <basis_universal/basisu_transcoder.h>
#include <oneapi/tbb/concurrent_hash_map.h>

#include <memory>
#include <variant>

namespace coffee {

    class AssetManager;
    using AssetManagerPtr = std::shared_ptr<AssetManager>;

    class AssetManager {
    public:
        static constexpr size_t UnderlyingBufferSize = 32;

        ~AssetManager() noexcept = default;

        static AssetManagerPtr create(const GPUDevicePtr& device);

        std::vector<uint8_t> getBytes(const std::string& path);
        std::vector<uint8_t> getBytes(const FilesystemPtr& fs, const std::string& path);
        ShaderPtr getShader(const std::string& path, ShaderStage stage, const std::string& entrypoint = "main");
        ShaderPtr getShader(const FilesystemPtr& fs, const std::string& path, ShaderStage stage, const std::string& entrypoint = "main");
        ModelPtr getModel(const std::string& path);
        ModelPtr getModel(const FilesystemPtr& fs, const std::string& path);
        // BEWARE: amountOfChannels ignored when image already loaded, you can extract actual amount of channels through VkFormat
        ImagePtr getImage(const std::string& path, uint32_t amountOfChannels);
        // BEWARE: amountOfChannels ignored for raw image types, you can extract actual amount of channels through VkFormat
        ImagePtr getImage(const FilesystemPtr& fs, const std::string& path, uint32_t amountOfChannels);

        void removeFromCache(const std::string& path);

    private:
        AssetManager(const GPUDevicePtr& device);
        void createMissingTexture();
        void selectOneChannel();
        void selectTwoChannels();
        void selectThreeChannels();
        void selectFourChannels();

        ModelPtr loadModel(const FilesystemPtr& filesystem, const std::string& path);
        std::string readMaterialName(utils::ReadOnlyStream<UnderlyingBufferSize>& stream);
        ImageViewPtr loadMaterial(const FilesystemPtr& filesystem, const std::string& path, uint32_t amountOfChannels);

        ImagePtr loadRawImage(std::vector<uint8_t>& rawBytes);
        ImagePtr loadBasisImage(std::vector<uint8_t>& rawBytes, uint32_t amountOfChannels);
        VkFormat channelsToVkFormat(uint32_t amountOfChannels, bool compressed);
        basist::transcoder_texture_format channelsToBasisuFormat(uint32_t amountOfChannels);

        struct Asset {
            Asset(std::vector<uint8_t> copyBytes) : type { Filesystem::FileType::Unknown }, actualObject { std::move(copyBytes) } {}

            Asset(ShaderPtr shader) : type { Filesystem::FileType::Shader }, actualObject { std::move(shader) } {}

            Asset(ModelPtr model) : type { Filesystem::FileType::Model }, actualObject { std::move(model) } {}

            Asset(ImagePtr image) : type { Filesystem::FileType::RawImage }, actualObject { std::move(image) } {}

            Filesystem::FileType type;
            std::variant<std::vector<uint8_t>, ShaderPtr, ModelPtr, ImagePtr> actualObject;
        };

        struct CompressionTypes {
            VkFormat vkOneChannel = VK_FORMAT_UNDEFINED;
            VkFormat vkTwoChannels = VK_FORMAT_UNDEFINED;
            VkFormat vkThreeChannels = VK_FORMAT_UNDEFINED;
            VkFormat vkFourChannels = VK_FORMAT_UNDEFINED;

            basist::transcoder_texture_format basisOneChannel = basist::transcoder_texture_format::cTFTotalTextureFormats;
            basist::transcoder_texture_format basisTwoChannels = basist::transcoder_texture_format::cTFTotalTextureFormats;
            basist::transcoder_texture_format basisThreeChannels = basist::transcoder_texture_format::cTFTotalTextureFormats;
            basist::transcoder_texture_format basisFourChannels = basist::transcoder_texture_format::cTFTotalTextureFormats;
        };

        GPUDevicePtr device_;
        ImagePtr missingImage_;
        ImageViewPtr missingTexture_;
        CompressionTypes compressionTypes_ {};

        tbb::concurrent_hash_map<XXH64_hash_t, Asset> cache_ {};

        using HashAccessor = tbb::concurrent_hash_map<XXH64_hash_t, AssetManager::Asset>::const_accessor;
    };

} // namespace coffee