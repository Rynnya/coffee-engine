#include <coffee/engine.hpp>

#include <coffee/objects/vertex.hpp>
#include <coffee/utils/log.hpp>
#include <coffee/utils/utils.hpp>

#define STB_IMAGE_IMPLEMENTATION

#include <GLFW/glfw3.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <glm/glm.hpp>
#include <glm/gtc/packing.hpp>
#include <stb_image.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace coffee {

    static Device* device_ = nullptr;
    static std::vector<std::vector<std::pair<VkCommandPool, VkCommandBuffer>>> poolsAndBuffers_ {};
    static std::vector<bool> poolsAndBuffersClearFlags_ {};
    static std::mutex poolsAndBuffersMutex_ {};

    static float framerateLimit_ = 60.0f;
    static float deltaTime_ = 60.0f / 1000.0f;
    static std::chrono::high_resolution_clock::time_point lastPollTime_ {};

    static Texture defaultTexture_ = nullptr;
    static std::array<std::unordered_map<std::string, Texture>, 7> loadedTextures_ {};

    static uint32_t nextMonitorID = 0;
    static std::vector<Monitor> monitors_ {};
    static std::unordered_map<std::string, std::function<void(const MonitorImpl&)>> monitorConnectedCallbacks_ {};
    static std::unordered_map<std::string, std::function<void(const MonitorImpl&)>> monitorDisconnectedCallbacks_ {};
    static std::mutex monitorConnectedMutex_ {};
    static std::mutex monitorDisconnectedMutex_ {};

    int cursorTypeToGLFWtype(CursorType type)
    {
        switch (type) {
            default:
            case CursorType::Arrow:
                return GLFW_ARROW_CURSOR;
            case CursorType::TextInput:
                return GLFW_IBEAM_CURSOR;
            case CursorType::CrossHair:
                return GLFW_CROSSHAIR_CURSOR;
            case CursorType::Hand:
                return GLFW_HAND_CURSOR;
            case CursorType::ResizeEW:
                return GLFW_RESIZE_EW_CURSOR;
            case CursorType::ResizeNS:
                return GLFW_RESIZE_NS_CURSOR;
            case CursorType::ResizeNWSE:
                return GLFW_RESIZE_NWSE_CURSOR;
            case CursorType::ResizeNESW:
                return GLFW_RESIZE_NESW_CURSOR;
            case CursorType::ResizeAll:
                return GLFW_RESIZE_ALL_CURSOR;
            case CursorType::NotAllowed:
                return GLFW_NOT_ALLOWED_CURSOR;
        }
    }

    template <size_t Size>
    class ReadOnlyStream {
    public:
        ReadOnlyStream(std::ifstream& inputStream) : inputStream_ { inputStream } {}

        ~ReadOnlyStream() noexcept = default;

        template <typename T>
        inline T read()
        {
            static_assert(!std::is_pointer_v<T> && !std::is_null_pointer_v<T>, "Don't use pointers, just use readBuffer.");
            COFFEE_ASSERT(Size >= sizeof(T), "Insufficient buffer, please increase it's template size.");

            static_cast<void>(inputStream_.read(underlyingBuffer_, sizeof(T)));
            return *reinterpret_cast<T*>(underlyingBuffer_);
        }

        template <typename T>
        inline T* readBuffer(size_t amount)
        {
            static_assert(!std::is_pointer_v<T> && !std::is_null_pointer_v<T>, "Don't use pointers");
            COFFEE_ASSERT(Size >= amount * sizeof(T), "Insufficient buffer, please increase it's template size.");

            static_cast<void>(inputStream_.read(underlyingBuffer_, amount * sizeof(T)));
            return reinterpret_cast<T*>(underlyingBuffer_);
        }

        template <typename T>
        inline void readDirectly(T* dstMemory, size_t amount = 1)
        {
            static_assert(!std::is_pointer_v<T> && !std::is_null_pointer_v<T>, "Don't use pointers");

            static_cast<void>(inputStream_.read(reinterpret_cast<char*>(dstMemory), amount * sizeof(T)));
        }

    private:
        char underlyingBuffer_[Size] {};
        std::ifstream& inputStream_;
    };

    void Engine::initialize()
    {
        COFFEE_ASSERT(device_ == nullptr, "Engine already initialized.");

        COFFEE_THROW_IF(glfwInit() != GLFW_TRUE, "Failed to initialize GLFW!");

        int32_t monitorCount {};
        GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);

        COFFEE_THROW_IF(monitorCount == 0, "Failed to get native monitors!");

        glfwSetMonitorCallback([](GLFWmonitor* monitor, int event) {
            if (event == GLFW_CONNECTED) {
                uint32_t thisMonitorID = nextMonitorID++;
                coffee::Monitor newMonitor = std::make_shared<MonitorImpl>(monitor, thisMonitorID);

                {
                    std::scoped_lock<std::mutex> lock { monitorConnectedMutex_ };

                    for (const auto& [name, callback] : monitorConnectedCallbacks_) {
                        callback(*newMonitor);
                    }
                }

                monitors_.push_back(std::move(newMonitor));
                glfwSetMonitorUserPointer(monitor, new uint32_t { thisMonitorID });

                int32_t monitorCount = 0;
                bool isFirst = glfwGetMonitors(&monitorCount)[0] == monitor;
                if (isFirst && monitorCount > 1) {
                    // Rotate last monitor and put it into first position
                    std::rotate(monitors_.rbegin(), monitors_.rbegin() + 1, monitors_.rend());
                }
            }
            else if (event == GLFW_DISCONNECTED) {
                uint32_t monitorID = *static_cast<uint32_t*>(glfwGetMonitorUserPointer(monitor));

                for (auto it = monitors_.begin(); it != monitors_.end(); it++) {
                    if ((*it)->uniqueID == monitorID) {
                        std::scoped_lock<std::mutex> lock { monitorDisconnectedMutex_ };

                        for (const auto& [name, callback] : monitorDisconnectedCallbacks_) {
                            callback(**it);
                        }

                        monitors_.erase(it);
                        break;
                    }
                }

                delete static_cast<uint32_t*>(glfwGetMonitorUserPointer(monitor));
            }
        });

        for (int32_t i = 0; i < monitorCount; i++) {
            uint32_t thisMonitorID = nextMonitorID++;
            glfwSetMonitorUserPointer(monitors[i], new uint32_t { thisMonitorID });
            monitors_.push_back(std::make_shared<MonitorImpl>(monitors[i], thisMonitorID));
        }

        device_ = new Device {};
        poolsAndBuffers_.resize(device_->imageCount());
        poolsAndBuffersClearFlags_.resize(device_->imageCount());

        createNullTexture();

        COFFEE_INFO("Engine initialized!");
    }

    void Engine::destroy() noexcept
    {
        COFFEE_ASSERT(device_ != nullptr, "Engine wasn't initialized/was de-initialized already.");

        glfwPollEvents();

        waitDeviceIdle();

        for (const auto& framePoolsAndBuffers : poolsAndBuffers_) {
            for (auto& [pool, commandBuffer] : framePoolsAndBuffers) {
                vkFreeCommandBuffers(device_->logicalDevice(), pool, 1U, &commandBuffer);
                device_->returnGraphicsCommandPool(pool);
            }
        }

        loadedTextures_.fill({});
        defaultTexture_ = nullptr;

        delete device_;
        device_ = nullptr;

        glfwTerminate();

        COFFEE_INFO("Engine de-initialized!");
    }

    Monitor Engine::primaryMonitor() noexcept
    {
        COFFEE_ASSERT(device_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");
        return monitors_[0];
    }

    const std::vector<Monitor>& Engine::monitors() noexcept
    {
        COFFEE_ASSERT(device_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");
        return monitors_;
    }

    void Engine::pollEvents()
    {
        COFFEE_ASSERT(device_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");
        glfwPollEvents();
    }

    void Engine::waitEventsTimeout(double timeout)
    {
        COFFEE_ASSERT(device_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");
        glfwWaitEventsTimeout(timeout);
    }

    void Engine::waitFramelimit()
    {
        using period = std::chrono::seconds::period;

        float frameTime = std::chrono::duration<float, period>(std::chrono::high_resolution_clock::now() - lastPollTime_).count();
        float waitForSeconds = std::max(1.0f / framerateLimit_ - frameTime, 0.0f);

        if (waitForSeconds > 0.0f) {
            auto spinStart = std::chrono::high_resolution_clock::now();
            while ((std::chrono::high_resolution_clock::now() - spinStart).count() / 1e9 < waitForSeconds)
                ;
        }

        auto currentTime = std::chrono::high_resolution_clock::now();
        deltaTime_ = std::chrono::duration<float, period>(currentTime - lastPollTime_).count();
        lastPollTime_ = currentTime;
    }

    void Engine::waitDeviceIdle()
    {
        COFFEE_ASSERT(device_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");
        vkDeviceWaitIdle(device_->logicalDevice());
    }

    float Engine::deltaTime() noexcept
    {
        return deltaTime_;
    }

    float Engine::framerateLimit() noexcept
    {
        return framerateLimit_;
    }

    void Engine::setFramerateLimit(float framerateLimit) noexcept
    {
        framerateLimit_ = framerateLimit;
    }

    Model Engine::importModel(const std::string& filename)
    {
        COFFEE_ASSERT(device_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");

        Assimp::Importer importer {};
        uint32_t processFlags =
            aiProcess_Triangulate | aiProcess_OptimizeMeshes | aiProcess_JoinIdenticalVertices |
            // aiProcess_MakeLeftHanded |
            aiProcess_GenNormals | aiProcess_CalcTangentSpace |
            // aiProcess_PreTransformVertices |
            aiProcess_FlipUVs;

        const aiScene* scene = importer.ReadFile(filename, processFlags);

        COFFEE_THROW_IF(scene == nullptr, "Failed to find proper loader for model ({})!", filename);
        COFFEE_THROW_IF(scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE, "Failed to fully load model ({})!", filename);
        COFFEE_THROW_IF(scene->mRootNode == nullptr, "Failed to load tree ({})!", filename);

        std::filesystem::path parentDirectory = std::filesystem::absolute(filename).parent_path();

        auto assimpTypeToEngineType = [](aiTextureType type) -> std::tuple<TextureType, VkFormat, int32_t> {
            switch (type) {
                default:
                case aiTextureType_NONE:
                    return { TextureType::None, VK_FORMAT_R8_UNORM, STBI_grey };
                case aiTextureType_DIFFUSE:
                    return { TextureType::Diffuse, VK_FORMAT_R8G8B8A8_SRGB, STBI_rgb_alpha };
                case aiTextureType_SPECULAR:
                    return { TextureType::Specular, VK_FORMAT_R8_UNORM, STBI_grey };
                case aiTextureType_NORMALS:
                    return { TextureType::Normals, VK_FORMAT_R8G8B8A8_UNORM, STBI_rgb_alpha };
                case aiTextureType_EMISSIVE:
                    return { TextureType::Emissive, VK_FORMAT_R8G8B8A8_SRGB, STBI_rgb_alpha };
                case aiTextureType_DIFFUSE_ROUGHNESS:
                    return { TextureType::Roughness, VK_FORMAT_R8_UNORM, STBI_grey };
                case aiTextureType_METALNESS:
                    return { TextureType::Metallic, VK_FORMAT_R8_UNORM, STBI_grey };
                case aiTextureType_AMBIENT_OCCLUSION:
                    return { TextureType::AmbientOcclusion, VK_FORMAT_R8_UNORM, STBI_grey };
            }
        };

        auto loadMaterialTextures =
            [&assimpTypeToEngineType,
             &parentDirectory](Materials& materials, const aiScene* scene, aiMaterial* material, aiTextureType textureType) -> void {
            auto&& [engineType, formatType, stbiType] = assimpTypeToEngineType(textureType);
            uint32_t materialTypeIndex = Math::indexOfHighestBit(static_cast<uint32_t>(engineType));
            COFFEE_ASSERT(materialTypeIndex >= 0 && materialTypeIndex <= 6, "Invalid TextureType provided.");

            for (uint32_t i = 0; i < material->GetTextureCount(textureType); i++) {
                aiString nativeMaterialPath {};
                material->GetTexture(textureType, i, &nativeMaterialPath);

                auto& materialMap = loadedTextures_[materialTypeIndex];
                std::string materialPath = (parentDirectory / nativeMaterialPath.data).generic_string();

                int32_t width {}, height {}, numberOfChannels {};
                Texture newTexture = nullptr;

                if (const aiTexture* embeddedTexture = scene->GetEmbeddedTexture(nativeMaterialPath.data)) {
                    size_t bufferSize = embeddedTexture->mWidth;

                    if (embeddedTexture->mHeight > 0) {
                        bufferSize *= embeddedTexture->mHeight;
                    }

                    uint8_t* rawBytes = stbi_load_from_memory(
                        reinterpret_cast<const uint8_t*>(embeddedTexture->pcData),
                        bufferSize,
                        &width,
                        &height,
                        &numberOfChannels,
                        stbiType
                    );
                    COFFEE_THROW_IF(rawBytes == nullptr, "STBI failed with reason: {}", stbi_failure_reason());

                    newTexture = createTexture(
                        rawBytes,
                        static_cast<size_t>(stbiType) * width * height,
                        formatType,
                        width,
                        height,
                        materialPath,
                        engineType
                    );
                    stbi_image_free(rawBytes);
                }
                else {
                    uint8_t* rawBytes = stbi_load(materialPath.c_str(), &width, &height, &numberOfChannels, stbiType);
                    COFFEE_THROW_IF(rawBytes == nullptr, "STBI failed with reason: {}", stbi_failure_reason());

                    newTexture = createTexture(
                        rawBytes,
                        static_cast<size_t>(stbiType) * width * height,
                        formatType,
                        width,
                        height,
                        materialPath,
                        engineType
                    );
                    stbi_image_free(rawBytes);
                }

                materials.write(newTexture);
                materialMap.emplace(materialPath, newTexture);
            }
        };

        auto loadSubmesh = [&assimpTypeToEngineType, &loadMaterialTextures](const aiScene* scene, aiMesh* mesh) -> Mesh {
            std::vector<Vertex> vertices {};
            std::vector<uint32_t> indices {};
            Materials materials { defaultTexture_ };

            for (uint32_t i = 0; i < mesh->mNumVertices; i++) {
                Vertex vertex {};

                vertex.position = glm::vec3 { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z } * 0.01f;

                if (mesh->HasNormals()) {
                    vertex.normal = { glm::packSnorm1x16(mesh->mNormals[i].x),
                                      glm::packSnorm1x16(mesh->mNormals[i].y),
                                      glm::packSnorm1x16(mesh->mNormals[i].z) };
                }

                if (mesh->HasTextureCoords(0)) {
                    vertex.texCoords = glm::packHalf2x16({ mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y });
                }

                if (mesh->HasTangentsAndBitangents()) {
                    vertex.tangent = { glm::packSnorm1x16(mesh->mTangents[i].x),
                                       glm::packSnorm1x16(mesh->mTangents[i].y),
                                       glm::packSnorm1x16(mesh->mTangents[i].z) };
                }

                vertices.push_back(std::move(vertex));
            }

            for (uint32_t i = 0; i < mesh->mNumFaces; i++) {
                const aiFace& face = mesh->mFaces[i];
                for (uint32_t j = 0; j < face.mNumIndices; j++) {
                    indices.push_back(face.mIndices[j]);
                }
            }

            if (scene->HasMaterials()) {
                aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

                loadMaterialTextures(materials, scene, material, aiTextureType_DIFFUSE);
                loadMaterialTextures(materials, scene, material, aiTextureType_SPECULAR);
                loadMaterialTextures(materials, scene, material, aiTextureType_NORMALS);
                loadMaterialTextures(materials, scene, material, aiTextureType_EMISSIVE);
                loadMaterialTextures(materials, scene, material, aiTextureType_DIFFUSE_ROUGHNESS);
                loadMaterialTextures(materials, scene, material, aiTextureType_METALNESS);
                loadMaterialTextures(materials, scene, material, aiTextureType_AMBIENT_OCCLUSION);

                aiColor3D diffuseColor { 1.0f, 1.0f, 1.0f };
                materials.modifiers.diffuseColor.r = diffuseColor.r;
                materials.modifiers.diffuseColor.g = diffuseColor.g;
                materials.modifiers.diffuseColor.b = diffuseColor.b;

                aiColor3D specularColor { 0.0f, 0.0f, 0.0f };
                materials.modifiers.specularColor.r = specularColor.r;
                materials.modifiers.specularColor.g = specularColor.g;
                materials.modifiers.specularColor.b = specularColor.b;

                material->Get(AI_MATKEY_METALLIC_FACTOR, materials.modifiers.metallicFactor);
                material->Get(AI_MATKEY_ROUGHNESS_FACTOR, materials.modifiers.roughnessFactor);
            }

            return std::make_unique<MeshImpl>(
                createVerticesBuffer(vertices.data(), vertices.size()),
                createIndicesBuffer(indices.data(), indices.size()),
                std::move(materials)
            );
        };

        std::vector<Mesh> meshes;

        auto processNode = [&meshes, &loadSubmesh](const aiScene* scene, aiNode* node, const auto& procNode) -> void {
            for (uint32_t i = 0; i < node->mNumMeshes; i++) {
                meshes.push_back(loadSubmesh(scene, scene->mMeshes[node->mMeshes[i]]));
            }

            for (uint32_t i = 0; i < node->mNumChildren; i++) {
                procNode(scene, node->mChildren[i], procNode);
            }
        };

        processNode(scene, scene->mRootNode, processNode);

        return std::make_shared<ModelImpl>(std::move(meshes));
    }

    Model Engine::importModel(const std::string& modelFile, const Archive& materialsArchive)
    {
        COFFEE_ASSERT(device_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");

        COFFEE_THROW_IF(modelFile.empty(), "modelFile is empty!");
        COFFEE_THROW_IF(!std::filesystem::exists(modelFile), "modelFile ({}) wasn't found!", modelFile);

        std::ifstream inputStream { modelFile, std::ios::binary | std::ios::in };
        COFFEE_THROW_IF(!inputStream.is_open(), "Failed to open a stream for file {}!", modelFile);

        constexpr uint8_t headerMagic[4] = { 0xF0, 0x7B, 0xAE, 0x31 };
        constexpr uint8_t meshMagic[4] = { 0x13, 0xEA, 0xB7, 0xF0 };
        ReadOnlyStream<32> stream { inputStream };

        COFFEE_THROW_IF(std::memcmp(stream.readBuffer<uint8_t>(4), headerMagic, 4) != 0, "Invalid header magic!");
        uint32_t meshesSize = stream.read<uint32_t>();
        std::vector<Mesh> meshes {};

        auto readMaterialName = [&stream]() -> std::string {
            uint8_t size = stream.read<uint8_t>();

            std::string outputString {};
            outputString.resize(size);

            stream.readDirectly(outputString.data(), size);
            return outputString;
        };

        for (uint32_t i = 0; i < meshesSize; i++) {
            COFFEE_THROW_IF(std::memcmp(stream.readBuffer<uint8_t>(4), meshMagic, 4) != 0, "Invalid mesh magic!");

            Materials materials { defaultTexture_ };
            uint32_t verticesSize = stream.read<uint32_t>();
            uint32_t indicesSize = stream.read<uint32_t>();
            uint32_t vertexSize = stream.read<uint32_t>();

            COFFEE_THROW_IF(vertexSize != sizeof(Vertex), "Vertex size isn't matching! Please update your model!");

            std::unique_ptr<Vertex[]> vertices = std::make_unique<Vertex[]>(verticesSize);
            std::unique_ptr<uint32_t[]> indices = std::make_unique<uint32_t[]>(indicesSize);

            stream.readDirectly(&materials.modifiers.diffuseColor);
            stream.readDirectly(&materials.modifiers.specularColor);
            stream.readDirectly(&materials.modifiers.metallicFactor);
            stream.readDirectly(&materials.modifiers.roughnessFactor);

            uint32_t textureFlags = stream.read<uint32_t>();

            std::string diffuseName = readMaterialName();
            std::string specularName = readMaterialName();
            std::string normalsName = readMaterialName();
            std::string emissiveName = readMaterialName();
            std::string roughnessName = readMaterialName();
            std::string metallicName = readMaterialName();
            std::string ambientOcclusionName = readMaterialName();

            if (!materialsArchive.isEmpty()) {
                auto findOrCreateTexture = [&materialsArchive, &materials](const std::string& name, TextureType type) -> void {
                    if (name.empty()) {
                        return;
                    }

                    const std::string completeFilepath = materialsArchive.getPath() + ":" + name;

                    auto& materialMap = loadedTextures_[Math::indexOfHighestBit(static_cast<uint32_t>(type))];
                    auto it = materialMap.find(completeFilepath);

                    if (it != materialMap.end()) {
                        materials.write(it->second);
                        return;
                    }

                    std::vector<uint8_t> materialData = materialsArchive.loadRawFile(name);

                    if (materialData.size() == 0) {
                        return;
                    }

                    [[maybe_unused]] auto verifyImageByteSize = [](uint32_t fileSize, TextureType type) {
                        switch (type) {
                            default:
                                return fileSize == 1;
                            case TextureType::Diffuse:
                            case TextureType::Emissive:
                            case TextureType::Normals:
                                return fileSize == 4;
                        }
                    };

                    uint32_t width = *reinterpret_cast<uint32_t*>(materialData.data());
                    uint32_t height = *reinterpret_cast<uint32_t*>(materialData.data() + sizeof(uint32_t));
                    uint32_t bytesPerPixel = *reinterpret_cast<uint32_t*>(materialData.data() + 2 * sizeof(uint32_t));
                    uint8_t* actualData = materialData.data() + 3 * sizeof(uint32_t);
                    size_t actualSize = materialData.size() - 3 * sizeof(uint32_t);

                    COFFEE_ASSERT(
                        verifyImageByteSize(bytesPerPixel, type),
                        "Image size mismatches with format size. Please repack your archive with "
                        "correct data."
                    );

                    Texture newTexture = createTexture(actualData, actualSize, typeToFormat(type), width, height, completeFilepath, type);

                    materialMap.emplace(completeFilepath, newTexture);
                    materials.write(newTexture);
                };

                findOrCreateTexture(diffuseName, TextureType::Diffuse);
                findOrCreateTexture(specularName, TextureType::Specular);
                findOrCreateTexture(normalsName, TextureType::Normals);
                findOrCreateTexture(emissiveName, TextureType::Emissive);
                findOrCreateTexture(roughnessName, TextureType::Roughness);
                findOrCreateTexture(metallicName, TextureType::Metallic);
                findOrCreateTexture(ambientOcclusionName, TextureType::AmbientOcclusion);
            }

            stream.readDirectly(vertices.get(), verticesSize);
            stream.readDirectly(indices.get(), indicesSize);

            meshes.push_back(std::make_unique<MeshImpl>(
                createVerticesBuffer(vertices.get(), verticesSize),
                createIndicesBuffer(indices.get(), indicesSize),
                std::move(materials)
            ));
        }

        return std::make_shared<ModelImpl>(std::move(meshes));
    }

    Texture Engine::importTexture(const std::string& filename, TextureType type)
    {
        COFFEE_ASSERT(device_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");

        uint32_t materialTypeIndex = static_cast<uint32_t>(type) - 1;
        COFFEE_ASSERT(materialTypeIndex >= 0 && materialTypeIndex < 10, "Invalid TextureType provided.");

        std::string absoluteFilePath = std::filesystem::absolute(filename).string();
        auto& materialMap = loadedTextures_[materialTypeIndex];
        auto iterator = materialMap.find(absoluteFilePath);

        if (iterator != materialMap.end()) {
            return iterator->second;
        }

        int32_t width {}, height {}, numberOfChannels {};

        uint8_t* rawBytes = stbi_load(absoluteFilePath.c_str(), &width, &height, &numberOfChannels, STBI_rgb_alpha);
        COFFEE_THROW_IF(rawBytes == nullptr, "STBI failed with reason: {}", stbi_failure_reason());

        Texture loadedTexture = createTexture(
            rawBytes,
            static_cast<size_t>(width) * height * 4,
            VK_FORMAT_R8G8B8A8_SRGB,
            width,
            height,
            absoluteFilePath,
            type
        );
        materialMap.emplace(absoluteFilePath, loadedTexture);

        stbi_image_free(rawBytes);
        return loadedTexture;
    }

    std::string_view Engine::getClipboard() noexcept
    {
        COFFEE_ASSERT(device_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");
        return std::string_view { glfwGetClipboardString(nullptr) };
    }

    void Engine::setClipboard(const std::string& clipboard) noexcept
    {
        COFFEE_ASSERT(device_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");
        glfwSetClipboardString(nullptr, clipboard.data());
    }

    void Engine::addMonitorConnectedCallback(const std::string& name, const std::function<void(const MonitorImpl&)>& callback)
    {
        COFFEE_ASSERT(device_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");

        if (callback == nullptr) {
            COFFEE_ERROR("Callback with name '{}' was discarded because it was nullptr", name);
            return;
        }

        std::scoped_lock<std::mutex> lock { monitorConnectedMutex_ };

        if (monitorConnectedCallbacks_.find(name) != monitorConnectedCallbacks_.end()) {
            COFFEE_WARNING("Callback with name '{}' was replaced", name);
        }

        monitorConnectedCallbacks_[name] = callback;
    }

    void Engine::removeMonitorConnectedCallback(const std::string& name)
    {
        COFFEE_ASSERT(device_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");

        std::scoped_lock<std::mutex> lock { monitorConnectedMutex_ };
        monitorConnectedCallbacks_.erase(name);
    }

    void Engine::addMonitorDisconnectedCallback(const std::string& name, const std::function<void(const MonitorImpl&)>& callback)
    {
        COFFEE_ASSERT(device_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");

        if (callback == nullptr) {
            COFFEE_ERROR("Callback with name '{}' was discarded because it was nullptr", name);
            return;
        }

        std::scoped_lock<std::mutex> lock { monitorDisconnectedMutex_ };

        if (monitorDisconnectedCallbacks_.find(name) != monitorDisconnectedCallbacks_.end()) {
            COFFEE_WARNING("Callback with name '{}' was replaced", name);
        }

        monitorDisconnectedCallbacks_[name] = callback;
    }

    void Engine::removeMonitorDisconnectedCallback(const std::string& name)
    {
        COFFEE_ASSERT(device_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");

        std::scoped_lock<std::mutex> lock { monitorDisconnectedMutex_ };
        monitorDisconnectedCallbacks_.erase(name);
    }

    void Engine::sendCommandBuffer(CommandBuffer&& commandBuffer)
    {
        SubmitInfo submitInfo {};

        {
            uint32_t currentOperation = device_->currentOperation();

            std::scoped_lock<std::mutex> lock { poolsAndBuffersMutex_ };

            if (poolsAndBuffersClearFlags_[currentOperation]) {
                device_->waitForAcquire();

                for (auto& [commandPool, commandBuffer] : poolsAndBuffers_[currentOperation]) {
                    vkFreeCommandBuffers(device_->logicalDevice(), commandPool, 1, &commandBuffer);
                    device_->returnGraphicsCommandPool(commandPool);
                }

                poolsAndBuffers_[currentOperation].clear();
                poolsAndBuffersClearFlags_[currentOperation] = false;
            }

            poolsAndBuffers_[currentOperation].push_back({ std::exchange(commandBuffer.pool_, VK_NULL_HANDLE), commandBuffer });

            COFFEE_THROW_IF(vkEndCommandBuffer(commandBuffer) != VK_SUCCESS, "Failed to end command buffer!");
            submitInfo.commandBuffers.push_back(commandBuffer);
        }

        device_->sendSubmitInfo(std::move(submitInfo));
    }

    void Engine::sendCommandBuffers(std::vector<CommandBuffer>&& commandBuffers)
    {
        SubmitInfo submitInfo {};

        {
            uint32_t currentOperation = device_->currentOperation();

            std::scoped_lock<std::mutex> lock { poolsAndBuffersMutex_ };

            if (poolsAndBuffersClearFlags_[currentOperation]) {
                device_->waitForAcquire();

                for (auto& [commandPool, commandBuffer] : poolsAndBuffers_[currentOperation]) {
                    vkFreeCommandBuffers(device_->logicalDevice(), commandPool, 1, &commandBuffer);
                    device_->returnGraphicsCommandPool(commandPool);
                }

                poolsAndBuffers_[currentOperation].clear();
                poolsAndBuffersClearFlags_[currentOperation] = false;
            }

            for (auto& commandBuffer : commandBuffers) {
                poolsAndBuffers_[currentOperation].push_back({ std::exchange(commandBuffer.pool_, VK_NULL_HANDLE), commandBuffer });

                COFFEE_THROW_IF(vkEndCommandBuffer(commandBuffer) != VK_SUCCESS, "Failed to end command buffer!");
                submitInfo.commandBuffers.push_back(commandBuffer);
            }
        }

        device_->sendSubmitInfo(std::move(submitInfo));
    }

    void Engine::submitPendingWork()
    {
        std::scoped_lock<std::mutex> lock { poolsAndBuffersMutex_ };

        // We must set this flag before submitting work, otherwise we will lock next vector of command buffers
        // This is something that we don't wanna because it will cause a chaos
        poolsAndBuffersClearFlags_[device_->currentOperation()] = true;

        device_->submitPendingWork();
    }

    size_t Engine::Factory::swapChainImageCount() noexcept
    {
        COFFEE_ASSERT(device_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");
        return device_->imageCount();
    }

    VkFormat Engine::Factory::surfaceColorFormat() noexcept
    {
        COFFEE_ASSERT(device_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");
        return device_->surfaceFormat().format;
    }

    Window Engine::Factory::createWindow(const WindowSettings& settings, const std::string& windowName)
    {
        COFFEE_ASSERT(device_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");
        return std::make_unique<WindowImpl>(*device_, settings, windowName);
    }

    Cursor Engine::Factory::createCursor(CursorType type) noexcept
    {
        COFFEE_ASSERT(device_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");

        if (GLFWcursor* cursor = glfwCreateStandardCursor(cursorTypeToGLFWtype(type))) {
            return std::make_shared<CursorImpl>(cursor, type);
        }

        return nullptr;
    }

    Cursor Engine::Factory::createCursorFromImage(
        const std::vector<uint8_t>& rawImage,
        uint32_t width,
        uint32_t height,
        CursorType type
    ) noexcept
    {
        COFFEE_ASSERT(device_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");

        if (width > std::numeric_limits<int32_t>::max() || height > std::numeric_limits<int32_t>::max()) {
            return nullptr;
        }

        constexpr size_t bytesPerPixel = 4;
        if (rawImage.size() < bytesPerPixel * width * height) {
            return nullptr;
        }

        GLFWimage image {};
        image.width = width;
        image.height = height;
        image.pixels = const_cast<uint8_t*>(rawImage.data()); // GLFW promises that they won't be modifying this data

        if (GLFWcursor* cursor = glfwCreateCursor(&image, 0, 0)) {
            return std::make_shared<CursorImpl>(cursor, type);
        }

        return nullptr;
    }

    Buffer Engine::Factory::createBuffer(const BufferConfiguration& configuration)
    {
        COFFEE_ASSERT(device_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");
        return std::make_unique<BufferImpl>(*device_, configuration);
    }

    Image Engine::Factory::createImage(const ImageConfiguration& configuration)
    {
        COFFEE_ASSERT(device_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");
        return std::make_shared<ImageImpl>(*device_, configuration);
    }

    Sampler Engine::Factory::createSampler(const SamplerConfiguration& configuration)
    {
        COFFEE_ASSERT(device_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");
        return std::make_shared<SamplerImpl>(*device_, configuration);
    }

    Shader Engine::Factory::createShader(const std::string& fileName, VkShaderStageFlagBits stage, const std::string& entrypoint)
    {
        COFFEE_ASSERT(device_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");
        return std::make_unique<ShaderImpl>(*device_, Utils::readFile(fileName), stage, entrypoint);
    }

    Shader Engine::Factory::createShader(const std::vector<uint8_t>& bytes, VkShaderStageFlagBits stage, const std::string& entrypoint)
    {
        COFFEE_ASSERT(device_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");
        return std::make_unique<ShaderImpl>(*device_, bytes, stage, entrypoint);
    }

    DescriptorLayout Engine::Factory::createDescriptorLayout(const std::map<uint32_t, DescriptorBindingInfo>& bindings)
    {
        COFFEE_ASSERT(device_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");
        return std::make_shared<DescriptorLayoutImpl>(*device_, bindings);
    }

    DescriptorSet Engine::Factory::createDescriptorSet(const DescriptorWriter& writer)
    {
        COFFEE_ASSERT(device_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");
        return std::make_shared<DescriptorSetImpl>(*device_, writer);
    }

    RenderPass Engine::Factory::createRenderPass(const RenderPassConfiguration& configuration)
    {
        COFFEE_ASSERT(device_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");
        return std::make_unique<RenderPassImpl>(*device_, configuration);
    }

    Pipeline Engine::Factory::createPipeline(
        const RenderPass& renderPass,
        const std::vector<DescriptorLayout>& descriptorLayouts,
        const std::vector<Shader>& shaderPrograms,
        const PipelineConfiguration& configuration
    )
    {
        COFFEE_ASSERT(device_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");

        [[maybe_unused]] constexpr auto verifyDescriptorLayouts = [](const std::vector<DescriptorLayout>& layouts) noexcept -> bool {
            bool result = true;

            for (const auto& layout : layouts) {
                result &= (layout != nullptr);
            }

            return result;
        };

        COFFEE_ASSERT(renderPass != nullptr, "Invalid RenderPass provided.");
        COFFEE_ASSERT(verifyDescriptorLayouts(descriptorLayouts), "Invalid std::vector<DescriptorLayout> provided.");

        return std::make_unique<PipelineImpl>(*device_, renderPass, descriptorLayouts, shaderPrograms, configuration);
    }

    Framebuffer Engine::Factory::createFramebuffer(const RenderPass& renderPass, const FramebufferConfiguration& configuration)
    {
        COFFEE_ASSERT(device_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");
        COFFEE_ASSERT(renderPass != nullptr, "Invalid RenderPass provided.");

        return std::make_unique<FramebufferImpl>(*device_, renderPass, configuration);
    }

    CommandBuffer Engine::Factory::createCommandBuffer()
    {
        COFFEE_ASSERT(device_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");
        return CommandBuffer { *device_, CommandBufferType::Graphics };
    }

    bool Engine::SingleTimeAction::isUnifiedTransferGraphicsQueue() noexcept
    {
        COFFEE_ASSERT(device_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");
        return device_->isUnifiedTransferGraphicsQueue();
    }

    uint32_t SingleTimeAction::transferQueueFamilyIndex() noexcept
    {
        COFFEE_ASSERT(device_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");
        return device_->transferQueueFamilyIndex();
    }

    uint32_t SingleTimeAction::graphicsQueueFamilyIndex() noexcept
    {
        COFFEE_ASSERT(device_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");
        return device_->graphicsQueueFamilyIndex();
    }

    ScopeExit Engine::SingleTimeAction::runTransfer(std::function<void(const CommandBuffer&)>&& action)
    {
        COFFEE_ASSERT(device_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");
        return device_->singleTimeTransfer(std::move(action));
    }

    ScopeExit Engine::SingleTimeAction::runGraphics(std::function<void(const CommandBuffer&)>&& action)
    {
        COFFEE_ASSERT(device_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");
        return device_->singleTimeGraphics(std::move(action));
    }

    void Engine::createNullTexture()
    {
        defaultTexture_ = createTexture(
            reinterpret_cast<const uint8_t*>("\255\255\255\255"),
            4,
            VK_FORMAT_R8G8B8A8_SRGB,
            1U,
            1U,
            "null",
            TextureType::None
        );
    }

    Texture Engine::createTexture(
        const uint8_t* rawBytes,
        size_t bufferSize,
        VkFormat format,
        uint32_t width,
        uint32_t height,
        const std::string& filePath,
        TextureType type
    )
    {
        COFFEE_ASSERT(rawBytes != nullptr, "rawBytes was nullptr.");

        ImageConfiguration textureConfiguration {};
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
        VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
        VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        textureConfiguration.format = format;
        textureConfiguration.extent.width = width;
        textureConfiguration.extent.height = height;
        textureConfiguration.tiling = VK_IMAGE_TILING_OPTIMAL;
        textureConfiguration.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        Image textureImage = std::make_shared<ImageImpl>(*device_, textureConfiguration);

        ImageViewConfiguration textureViewConfiguration {};
        textureViewConfiguration.viewType = VK_IMAGE_VIEW_TYPE_2D;
        textureViewConfiguration.format = format;
        textureViewConfiguration.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        textureViewConfiguration.subresourceRange.baseMipLevel = 0;
        textureViewConfiguration.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        textureViewConfiguration.subresourceRange.baseArrayLayer = 0;
        textureViewConfiguration.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
        ImageView textureView = std::make_shared<ImageViewImpl>(textureImage, textureViewConfiguration);

        BufferConfiguration stagingBufferConfiguration {};
        uint32_t instanceSize = 1U;
        uint32_t instanceCount = 1U;
        VkBufferUsageFlags usageFlags = 0;
        VkMemoryPropertyFlags memoryProperties = 0;
        stagingBufferConfiguration.usageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        stagingBufferConfiguration.memoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        stagingBufferConfiguration.instanceCount = 1U;
        stagingBufferConfiguration.instanceSize = bufferSize;
        stagingBufferConfiguration.allocationFlags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        Buffer stagingBuffer = std::make_unique<BufferImpl>(*device_, stagingBufferConfiguration);

        stagingBuffer->map();
        {
            std::memcpy(stagingBuffer->memory(), rawBytes, bufferSize);
            stagingBuffer->flush();
        }
        stagingBuffer->unmap();

        coffee::SingleTimeAction::copyBufferToImage(textureImage, stagingBuffer);

        return std::make_shared<TextureImpl>(std::move(textureImage), std::move(textureView), filePath, type);
    }

    Buffer Engine::createVerticesBuffer(const Vertex* vertices, size_t amount)
    {
        BufferConfiguration stagingBufferConfiguration {};
        stagingBufferConfiguration.usageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        stagingBufferConfiguration.memoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        stagingBufferConfiguration.instanceCount = amount;
        stagingBufferConfiguration.instanceSize = sizeof(Vertex);
        stagingBufferConfiguration.allocationFlags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        Buffer stagingVerticesBuffer = std::make_unique<BufferImpl>(*device_, stagingBufferConfiguration);

        stagingVerticesBuffer->map();
        {
            std::memcpy(stagingVerticesBuffer->memory(), vertices, amount * sizeof(Vertex));
            stagingVerticesBuffer->flush();
        }
        stagingVerticesBuffer->unmap();

        BufferConfiguration verticesBufferConfiguration {};
        verticesBufferConfiguration.usageFlags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        verticesBufferConfiguration.memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        verticesBufferConfiguration.instanceCount = amount;
        verticesBufferConfiguration.instanceSize = sizeof(Vertex);
        Buffer verticesBuffer = std::make_unique<BufferImpl>(*device_, verticesBufferConfiguration);

        coffee::SingleTimeAction::copyBuffer(verticesBuffer, stagingVerticesBuffer);

        return verticesBuffer;
    }

    Buffer Engine::createIndicesBuffer(const uint32_t* indices, size_t amount)
    {
        if (amount == 0) {
            return nullptr;
        }

        BufferConfiguration stagingBufferConfiguration {};
        stagingBufferConfiguration.usageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        stagingBufferConfiguration.memoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        stagingBufferConfiguration.instanceCount = amount;
        stagingBufferConfiguration.instanceSize = sizeof(uint32_t);
        stagingBufferConfiguration.allocationFlags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        Buffer stagingIndicesBuffer = std::make_unique<BufferImpl>(*device_, stagingBufferConfiguration);

        stagingIndicesBuffer->map();
        {
            std::memcpy(stagingIndicesBuffer->memory(), indices, amount * sizeof(uint32_t));
            stagingIndicesBuffer->flush();
        }
        stagingIndicesBuffer->unmap();

        BufferConfiguration indicesBufferConfiguration {};
        indicesBufferConfiguration.usageFlags = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        indicesBufferConfiguration.memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        indicesBufferConfiguration.instanceCount = amount;
        indicesBufferConfiguration.instanceSize = sizeof(uint32_t);
        Buffer indicesBuffer = std::make_unique<BufferImpl>(*device_, indicesBufferConfiguration);

        coffee::SingleTimeAction::copyBuffer(indicesBuffer, stagingIndicesBuffer);

        return indicesBuffer;
    }

    VkFormat Engine::typeToFormat(TextureType type) noexcept
    {
        switch (type) {
            default:
                return VK_FORMAT_R8_UNORM;
            case TextureType::Diffuse:
            case TextureType::Emissive:
                return VK_FORMAT_R8G8B8A8_SRGB;
            case TextureType::Normals:
                return VK_FORMAT_R8G8B8A8_UNORM;
        }
    }

} // namespace coffee