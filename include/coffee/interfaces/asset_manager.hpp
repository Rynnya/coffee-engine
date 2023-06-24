#ifndef COFFEE_INTERFACES_ASSET_MANAGER
#define COFFEE_INTERFACES_ASSET_MANAGER

#include <coffee/graphics/image.hpp>
#include <coffee/graphics/shader.hpp>
#include <coffee/objects/model.hpp>

#include <coffee/interfaces/filesystem.hpp>
#include <coffee/interfaces/resource_guard.hpp>
#include <coffee/utils/utils.hpp>

#include <basis_universal/basisu_transcoder.h>
#include <oneapi/tbb/concurrent_unordered_map.h>
#include <oneapi/tbb/queuing_mutex.h>

#include <memory>
#include <queue>
#include <variant>

namespace coffee {

    class AssetManager;
    using AssetManagerPtr = std::shared_ptr<AssetManager>;

    class AssetManager {
    private:
        using ShaderStage = graphics::ShaderStage;

    public:
        ~AssetManager() noexcept = default;

        static AssetManagerPtr create(const graphics::DevicePtr& device);

        std::vector<uint8_t> getBytes(const std::string& path);
        std::vector<uint8_t> getBytes(const FilesystemPtr& fs, const std::string& path);
        graphics::ShaderPtr getShader(const std::string& path, ShaderStage stage, const std::string& entrypoint = "main");
        graphics::ShaderPtr getShader(const FilesystemPtr& fs, const std::string& path, ShaderStage stage, const std::string& e = "main");
        ModelPtr getModel(const std::string& path);
        ModelPtr getModel(const FilesystemPtr& fs, const std::string& path);
        // BEWARE: amountOfChannels ignored when image already loaded, you can extract actual amount of channels through VkFormat
        graphics::ImagePtr getImage(const std::string& path, uint32_t amountOfChannels);
        // BEWARE: amountOfChannels ignored for raw image types, you can extract actual amount of channels through VkFormat
        graphics::ImagePtr getImage(const FilesystemPtr& fs, const std::string& path, uint32_t amountOfChannels);
        void getSound(const std::string& path);
        void getSound(const FilesystemPtr& fs, const std::string& path);
        void getAudioStream(const std::string& path);
        void getAudioStream(const FilesystemPtr& fs, const std::string& path);

        // Thread-safe remove function, may cause blocking
        void removeFromCache(const std::string& path);

    private:
        AssetManager(const graphics::DevicePtr& device);
        void createMissingTexture();
        void selectOneChannel();
        void selectTwoChannels();
        void selectThreeChannels();
        void selectFourChannels();

        ModelPtr loadModel(const FilesystemPtr& filesystem, const std::string& path);
        std::string readMaterialName(utils::ReaderStream& stream);
        graphics::ImageViewPtr loadMaterial(const FilesystemPtr& filesystem, const std::string& path, uint32_t amountOfChannels);

        ResourceGuard<graphics::ImagePtr> loadImageUnsafe(const FilesystemPtr& fs, const std::string& path, uint32_t amountOfChannels);
        ResourceGuard<graphics::ImagePtr> loadRawImage(std::vector<uint8_t>& rawBytes);
        ResourceGuard<graphics::ImagePtr> loadBasisImage(std::vector<uint8_t>& rawBytes, uint32_t amountOfChannels);
        VkFormat channelsToVkFormat(uint32_t amountOfChannels, bool compressed);
        basist::transcoder_texture_format channelsToBasisuFormat(uint32_t amountOfChannels);

        struct Asset {
            Asset(std::vector<uint8_t> copyBytes) : type { Filesystem::FileType::RawBytes }, actualObject { std::move(copyBytes) } {}

            Asset(graphics::ShaderPtr shader) : type { Filesystem::FileType::Shader }, actualObject { std::move(shader) } {}

            Asset(ModelPtr model) : type { Filesystem::FileType::Model }, actualObject { std::move(model) } {}

            Asset(graphics::ImagePtr image) : type { Filesystem::FileType::RawImage }, actualObject { std::move(image) } {}

            Filesystem::FileType type;
            std::variant<std::vector<uint8_t>, graphics::ShaderPtr, ModelPtr, graphics::ImagePtr> actualObject;
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

        graphics::DevicePtr device_;
        graphics::ImagePtr missingImage_;
        graphics::ImageViewPtr missingTexture_;
        CompressionTypes compressionTypes_ {};

        tbb::queuing_mutex mutex_ {};
        tbb::concurrent_unordered_map<XXH64_hash_t, Asset> cache_ {};
    };

} // namespace coffee

#endif