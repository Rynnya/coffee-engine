#ifndef COFFEE_INTERFACES_ASSET_MANAGER
#define COFFEE_INTERFACES_ASSET_MANAGER

#include <coffee/graphics/image.hpp>
#include <coffee/graphics/mesh.hpp>
#include <coffee/graphics/shader.hpp>

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

    // Q: Why not just use inheritance to simply all this info structs?
    // A: Designated initializers doesn't work well with inheritance, thus all fields repeatedly provided

    struct BytesLoadingInfo {
        // Filesystem as fallback loading method, if cache doesn't contain requested asset
        FilesystemPtr filesystem = nullptr;
        // Path to requested asset
        std::string path = {};
    };

    struct ShaderLoadingInfo {
        // Filesystem as fallback loading method, if cache doesn't contain requested asset
        FilesystemPtr filesystem = nullptr;
        // Path to requested asset
        std::string path = {};
        // Shader entrypoint, almost always it's just "main"
        std::string entrypoint = "main";
    };

    struct ImageLoadingInfo {
        // Filesystem as fallback loading method, if cache doesn't contain requested asset
        FilesystemPtr filesystem = nullptr;
        // Path to requested asset
        std::string path = {};
    };

    struct MeshLoadingInfo {
        // Filesystem as fallback loading method, if cache doesn't contain requested asset
        FilesystemPtr filesystem = nullptr;
        // Path to requested asset
        std::string path = {};
    };

    struct SoundLoadingInfo {
        // Filesystem as fallback loading method, if cache doesn't contain requested asset
        FilesystemPtr filesystem = nullptr;
        // Path to requested asset
        std::string path = {};
    };

    struct AudioStreamLoadingInfo {
        // Filesystem as fallback loading method, if cache doesn't contain requested asset
        FilesystemPtr filesystem = nullptr;
        // Path to requested asset
        std::string path = {};
    };

    // Asynchronous loader for coffee::Filesystem
    // Calling any of functions below is thread-safe unless otherwise specified
    class AssetManager {
    public:
        ~AssetManager() noexcept = default;

        static AssetManagerPtr create(const graphics::DevicePtr& device);

        std::vector<uint8_t> loadBytes(const BytesLoadingInfo& loadingInfo);
        graphics::ShaderPtr loadShader(const ShaderLoadingInfo& loadingInfo);
        graphics::ImagePtr loadImage(const ImageLoadingInfo& loadingInfo);
        graphics::MeshPtr loadMesh(const MeshLoadingInfo& loadingInfo);
        void loadSound(const SoundLoadingInfo& loadingInfo);
        void loadAudioStream(const AudioStreamLoadingInfo& loadingInfo);

        // Thread-safe remove function, may cause blocking
        void removeFromCache(const std::string& path);

    private:
        AssetManager(const graphics::DevicePtr& device);
        void createMissingTexture();
        void selectOneChannel();
        void selectTwoChannels();
        void selectThreeChannels();
        void selectFourChannels();

        graphics::MeshPtr loadMesh(const FilesystemPtr& filesystem, const std::string& path);
        std::string readMaterialName(utils::ReaderStream& stream);

        graphics::ImagePtr loadRawImage(std::vector<uint8_t>& rawBytes);
        graphics::ImagePtr loadBasisImage(std::vector<uint8_t>& rawBytes);

        VkFormat channelsToVkFormat(uint32_t amountOfChannels, bool compressed);
        basist::transcoder_texture_format channelsToBasisuFormat(uint32_t amountOfChannels);

        struct Asset {
            static Asset create(std::shared_ptr<std::vector<uint8_t>> copyBytes) { return { Filesystem::FileType::RawBytes, std::move(copyBytes) }; }

            static Asset create(graphics::ShaderPtr shader) { return { Filesystem::FileType::Shader, std::move(shader) }; }

            static Asset create(graphics::MeshPtr mesh) { return { Filesystem::FileType::Mesh, std::move(mesh) }; }

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
            graphics::AABB aabb;
            uint32_t verticesOffset;
            uint32_t verticesSize;
            uint32_t indicesOffset;
            uint32_t indicesSize;
        };

        struct MaterialMetadata {
            graphics::Materials* materials;
            std::string name;
            TextureType type;
        };

        struct MipmapInformation {
            size_t bufferOffset = 0;
            uint32_t width = 0;
            uint32_t height = 0;
        };

        graphics::DevicePtr device_;
        graphics::ImagePtr missingImage_;
        graphics::ImageViewPtr missingTexture_;
        CompressionTypes compressionTypes_ {};

        using HashAccessor = tbb::concurrent_hash_map<XXH64_hash_t, Asset>::const_accessor;
        tbb::concurrent_hash_map<XXH64_hash_t, Asset> cache_ {};
    };

} // namespace coffee

#endif