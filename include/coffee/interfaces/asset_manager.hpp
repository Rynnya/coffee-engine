#ifndef COFFEE_INTERFACES_ASSET_MANAGER
#define COFFEE_INTERFACES_ASSET_MANAGER

#include <coffee/graphics/image.hpp>
#include <coffee/graphics/shader.hpp>
#include <coffee/objects/model.hpp>

#include <coffee/interfaces/filesystem.hpp>
#include <coffee/interfaces/scope_guard.hpp>
#include <coffee/utils/utils.hpp>

#include <basis_universal/basisu_transcoder.h>
#include <oneapi/tbb/blocked_range.h>
#include <oneapi/tbb/concurrent_hash_map.h>

#include <memory>
#include <queue>
#include <variant>

namespace coffee {

    class AssetManager;
    using AssetManagerPtr = std::shared_ptr<AssetManager>;

    // Asynchronous loader for coffee::Filesystem
    // Calling any of functions below is thread-safe unless otherwise specified
    // Function DO NOT wait for operations to complete, you must explicitly call waitDeviceIdle or waitQueue*Idle
    class AssetManager {
    public:
        ~AssetManager() noexcept = default;

        static AssetManagerPtr create(const graphics::DevicePtr& device);

        std::vector<uint8_t> getBytes(const std::string& path);
        std::vector<uint8_t> getBytes(const FilesystemPtr& fs, const std::string& path);
        graphics::ShaderPtr getShader(const std::string& path, const std::string& entrypoint = "main");
        graphics::ShaderPtr getShader(const FilesystemPtr& fs, const std::string& path, const std::string& entrypoint = "main");
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

        graphics::ImagePtr loadRawImage(std::vector<uint8_t>& rawBytes);
        graphics::ImagePtr loadBasisImage(std::vector<uint8_t>& rawBytes, uint32_t amountOfChannels);
        VkFormat channelsToVkFormat(uint32_t amountOfChannels, bool compressed);
        basist::transcoder_texture_format channelsToBasisuFormat(uint32_t amountOfChannels);

        struct Asset {
            static Asset create(std::shared_ptr<std::vector<uint8_t>> copyBytes) { return { Filesystem::FileType::RawBytes, std::move(copyBytes) }; }

            static Asset create(graphics::ShaderPtr shader) { return { Filesystem::FileType::Shader, std::move(shader) }; }

            static Asset create(ModelPtr model) { return { Filesystem::FileType::Model, std::move(model) }; }

            static Asset create(graphics::ImagePtr image) { return { Filesystem::FileType::RawImage, std::move(image) }; }

            Filesystem::FileType type;
            std::shared_ptr<void> actualObject;
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

        struct MeshMetadata {
            AABB aabb;
            uint32_t verticesOffset;
            uint32_t verticesSize;
            uint32_t indicesOffset;
            uint32_t indicesSize;
        };

        class ModelLoader {
        public:
            struct MaterialMetadata {
                Materials* materials;
                std::string name;
                uint32_t amountOfChannels;
                TextureType type;
            };

            struct MipmapInformation {
                size_t bufferOffset = 0;
                uint32_t width = 0;
                uint32_t height = 0;
            };

            ModelLoader(AssetManager& manager, const FilesystemPtr& filesystem, std::vector<MaterialMetadata>& materialsMetadata);

            void operator()(const tbb::blocked_range<size_t>& range) const;

            graphics::ImagePtr loadRawImage(
                graphics::BufferPtr& stagingBuffer,
                const graphics::FencePtr& transferFence,
                std::vector<uint8_t>& rawBytes
            ) const;
            graphics::ImagePtr loadBasisImage(
                graphics::BufferPtr& stagingBuffer,
                const graphics::FencePtr& transferFence,
                std::vector<uint8_t>& rawBytes,
                uint32_t amountOfChannels
            ) const;

        private:
            AssetManager& manager;
            const FilesystemPtr& filesystem;
            std::vector<MaterialMetadata>& materialsMetadata;
        };

        graphics::DevicePtr device_;
        graphics::ImagePtr missingImage_;
        graphics::ImageViewPtr missingTexture_;
        CompressionTypes compressionTypes_ {};

        using HashAccessor = tbb::concurrent_hash_map<XXH64_hash_t, Asset>::const_accessor;
        tbb::concurrent_hash_map<XXH64_hash_t, Asset> cache_ {};

        friend class ModelLoader;
    };

} // namespace coffee

#endif