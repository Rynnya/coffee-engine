#include <coffee/engine.hpp>

#include <coffee/abstract/vulkan/vk_backend.hpp>
#include <coffee/objects/vertex.hpp>
#include <coffee/utils/log.hpp>

#define STB_IMAGE_IMPLEMENTATION

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/packing.hpp>
#include <stb_image.h>

#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <fstream>

namespace coffee {

    static std::unique_ptr<AbstractBackend> backendImpl_ = nullptr;

    static float framerateLimit_ = 60.0f;
    static float deltaTime_ = 1.0f / 60.0f;
    static std::chrono::high_resolution_clock::time_point lastPollTime_ {};

    static Texture defaultTexture_ = nullptr;
    static std::array<std::unordered_map<std::string, Texture>, 7> loadedTextures_ {};

    uint32_t nextMonitorID = 0;
    std::vector<Monitor> monitors_ {};
    static std::unordered_map<std::string, std::function<void(const MonitorImpl&)>> monitorConnectedCallbacks_ {};
    static std::unordered_map<std::string, std::function<void(const MonitorImpl&)>> monitorDisconnectedCallbacks_ {};
    static std::mutex monitorConnectedMutex_ {};
    static std::mutex monitorDisconnectedMutex_ {};

    template <size_t Size>
    class ReadOnlyStream {
    public:
        ReadOnlyStream(std::ifstream& inputStream) : inputStream_ { inputStream } {}
        ~ReadOnlyStream() noexcept = default;

        template <typename T>
        inline T read() {
            static_assert(!std::is_pointer_v<T> && !std::is_null_pointer_v<T>, "Don't use pointers, just use readBuffer.");
            COFFEE_ASSERT(Size >= sizeof(T), "Insufficient buffer, please increase it's template size.");

            static_cast<void>(inputStream_.read(underlyingBuffer_, sizeof(T)));
            return *reinterpret_cast<T*>(underlyingBuffer_);
        }

        template <typename T>
        inline T* readBuffer(size_t amount) {
            static_assert(!std::is_pointer_v<T> && !std::is_null_pointer_v<T>, "Don't use pointers");
            COFFEE_ASSERT(Size >= amount * sizeof(T), "Insufficient buffer, please increase it's template size.");

            static_cast<void>(inputStream_.read(underlyingBuffer_, amount * sizeof(T)));
            return reinterpret_cast<T*>(underlyingBuffer_);
        }

    private:
        char underlyingBuffer_[Size] {};
        std::ifstream& inputStream_;
    };

    void Engine::initialize(BackendAPI backend) {
        COFFEE_ASSERT(backendImpl_ == nullptr, "Engine already initialized.");

        COFFEE_THROW_IF(glfwInit() != GLFW_TRUE, "Failed to initialize GLFW!");

        int32_t monitorCount {};
        GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);

        COFFEE_THROW_IF(monitorCount == 0, "Failed to get native monitors!");

        glfwSetMonitorCallback([](GLFWmonitor* monitor, int event) {
            if (event == GLFW_CONNECTED) {
                uint32_t thisMonitorID = nextMonitorID++;
                coffee::Monitor newMonitor = std::make_shared<MonitorImpl>(static_cast<void*>(monitor), thisMonitorID);

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
                    if ((*it)->getUniqueID() == monitorID) {
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
            monitors_.push_back(std::make_shared<MonitorImpl>(static_cast<void*>(monitors[i]), thisMonitorID));
        }

        switch (backend) {
            case BackendAPI::Vulkan:
                backendImpl_ = std::make_unique<VulkanBackend>();
                break;
            case BackendAPI::D3D12:
                throw std::runtime_error("D3D12 is not implemented.");
                break;
            default:
                COFFEE_ASSERT(false, "Invalid BackendAPI provided. This should not happen.");
                throw std::runtime_error("Invalid BackendAPI provided.");
                break;
        }

        createNullTexture();

        COFFEE_INFO("Engine initialized!");
    }

    void Engine::destroy() noexcept {
        COFFEE_ASSERT(backendImpl_ != nullptr, "Engine wasn't initialized/was de-initialized already.");

        glfwPollEvents();

        waitDeviceIdle();
        loadedTextures_.fill({});
        defaultTexture_ = nullptr;
        backendImpl_ = nullptr;

        glfwTerminate();

        COFFEE_INFO("Engine de-initialized!");
    }

    BackendAPI Engine::getBackendType() noexcept {
        COFFEE_ASSERT(backendImpl_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");
        return backendImpl_->getBackendType();
    }

    Monitor Engine::getPrimaryMonitor() noexcept {
        COFFEE_ASSERT(backendImpl_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");
        return monitors_[0];
    }

    const std::vector<Monitor>& Engine::getMonitors() noexcept {
        COFFEE_ASSERT(backendImpl_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");
        return monitors_;
    }

    void Engine::pollEvents() {
        COFFEE_ASSERT(backendImpl_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");
        glfwPollEvents();
    }

    void Engine::wait() {
        float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(
            std::chrono::high_resolution_clock::now() - lastPollTime_).count();
        float waitForSeconds = std::max(1.0f / framerateLimit_ - frameTime, 0.0f);

        if (waitForSeconds > 0.0f) {
            auto spinStart = std::chrono::high_resolution_clock::now();
            while ((std::chrono::high_resolution_clock::now() - spinStart).count() / 1e9 < waitForSeconds);
        }

        auto currentTime = std::chrono::high_resolution_clock::now();
        deltaTime_ = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - lastPollTime_).count();
        lastPollTime_ = currentTime;
    }

    float Engine::getDeltaTime() noexcept {
        return deltaTime_;
    }

    float Engine::getFramerateLimit() noexcept {
        return framerateLimit_;
    }

    void Engine::setFramerateLimit(float framerateLimit) noexcept {
        framerateLimit_ = framerateLimit;
    }

    Model Engine::importModel(const std::string& filename) {
        COFFEE_ASSERT(backendImpl_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");

        Assimp::Importer importer {};
        uint32_t processFlags =
            aiProcess_Triangulate |
            aiProcess_OptimizeMeshes |
            aiProcess_JoinIdenticalVertices |
            //aiProcess_MakeLeftHanded |
            aiProcess_GenNormals |
            aiProcess_CalcTangentSpace |
            //aiProcess_PreTransformVertices |
            aiProcess_FlipUVs;

        const aiScene* scene = importer.ReadFile(filename, processFlags);

        COFFEE_THROW_IF(scene == nullptr, "Failed to find proper loader for model ({})!", filename);
        COFFEE_THROW_IF(scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE, "Failed to fully load model ({})!", filename);
        COFFEE_THROW_IF(scene->mRootNode == nullptr, "Failed to load tree ({})!", filename);

        std::filesystem::path parentDirectory = std::filesystem::absolute(filename).parent_path();

        auto assimpTypeToEngineType = [](aiTextureType type) -> std::tuple<TextureType, Format, int32_t> {
            switch (type) {
                default:
                case aiTextureType_NONE:
                    return { TextureType::None, Format::R8UNorm, STBI_grey };
                case aiTextureType_DIFFUSE:
                    return { TextureType::Diffuse, Format::R8G8B8A8SRGB, STBI_rgb_alpha };
                case aiTextureType_SPECULAR:
                    return { TextureType::Specular, Format::R8UNorm, STBI_grey };
                case aiTextureType_NORMALS:
                    return { TextureType::Normals, Format::R8G8B8A8UNorm, STBI_rgb_alpha };
                case aiTextureType_EMISSIVE:
                    return { TextureType::Emissive, Format::R8G8B8A8SRGB, STBI_rgb_alpha };
                case aiTextureType_DIFFUSE_ROUGHNESS:
                    return { TextureType::Roughness, Format::R8UNorm, STBI_grey };
                case aiTextureType_METALNESS:
                    return { TextureType::Metallic, Format::R8UNorm, STBI_grey };
                case aiTextureType_AMBIENT_OCCLUSION:
                    return { TextureType::AmbientOcclusion, Format::R8UNorm, STBI_grey };
            }
        };

        auto loadMaterialTextures = [&assimpTypeToEngineType, &parentDirectory](
            Materials& materials,
            const aiScene* scene,
            aiMaterial* material,
            aiTextureType textureType
        ) -> void {
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
                        reinterpret_cast<const uint8_t*>(embeddedTexture->pcData), bufferSize, &width, &height, &numberOfChannels, stbiType);
                    COFFEE_THROW_IF(rawBytes == nullptr, "STBI failed with reason: {}", stbi_failure_reason());

                    newTexture = createTexture(
                        rawBytes, static_cast<size_t>(stbiType) * width * height, formatType, width, height, materialPath, engineType);
                    stbi_image_free(rawBytes);
                }
                else {
                    uint8_t* rawBytes = stbi_load(materialPath.c_str(), &width, &height, &numberOfChannels, stbiType);
                    COFFEE_THROW_IF(rawBytes == nullptr, "STBI failed with reason: {}", stbi_failure_reason());

                    newTexture = createTexture(
                        rawBytes, static_cast<size_t>(stbiType) * width * height, formatType, width, height, materialPath, engineType);
                    stbi_image_free(rawBytes);
                }

                materials.write(newTexture);
                materialMap.emplace(materialPath, newTexture);
            }
        };

        auto loadSubmesh = [&assimpTypeToEngineType, &loadMaterialTextures](
            const aiScene* scene, 
            aiMesh* mesh
        ) -> Mesh {
            std::vector<Vertex> vertices {};
            std::vector<uint32_t> indices {};
            Materials materials { defaultTexture_ };

            for (uint32_t i = 0; i < mesh->mNumVertices; i++) {
                Vertex vertex {};

                vertex.position = glm::vec3 { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z } * 0.01f;

                if (mesh->HasNormals()) {
                    vertex.normal = {
                        glm::packHalf1x16(mesh->mNormals[i].x),
                        glm::packHalf1x16(mesh->mNormals[i].y),
                        glm::packHalf1x16(mesh->mNormals[i].z)
                    };
                }

                if (mesh->HasTextureCoords(0)) {
                    vertex.texCoords = glm::packHalf2x16({ mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y });
                }

                if (mesh->HasTangentsAndBitangents()) {
                    vertex.tangent = {
                        glm::packHalf1x16(mesh->mTangents[i].x),
                        glm::packHalf1x16(mesh->mTangents[i].y),
                        glm::packHalf1x16(mesh->mTangents[i].z)
                    };
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

            return std::make_unique<MeshImpl>(createVerticesBuffer(vertices), createIndicesBuffer(indices), std::move(materials));
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

    Model Engine::importModel(const std::string& modelFile, const Archive& materialsArchive) {
        COFFEE_ASSERT(backendImpl_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");

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
            return std::string { stream.readBuffer<char>(size), size };
        };

        for (uint32_t i = 0; i < meshesSize; i++) {
            COFFEE_THROW_IF(std::memcmp(stream.readBuffer<uint8_t>(4), meshMagic, 4) != 0, "Invalid mesh magic!");

            std::vector<Vertex> vertices {};
            std::vector<uint32_t> indices {};
            Materials materials { defaultTexture_ };
            uint32_t verticesSize = stream.read<uint32_t>();
            uint32_t indicesSize = stream.read<uint32_t>();
            uint32_t vertexSize = stream.read<uint32_t>();

            COFFEE_THROW_IF(vertexSize != sizeof(Vertex), "Vertex size isn't matching! Please update your model!");

            vertices.reserve(verticesSize);
            indices.reserve(indicesSize);

            materials.modifiers.diffuseColor = stream.read<glm::vec3>();
            materials.modifiers.specularColor = stream.read<glm::vec3>();
            materials.modifiers.metallicFactor = stream.read<glm::float32>();
            materials.modifiers.roughnessFactor = stream.read<glm::float32>();

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

                    [[ maybe_unused ]] auto verifyImageByteSize = [](uint32_t fileSize, TextureType type) {
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
                        "Image size mismatches with format size. Please repack your archive with correct data.");

                    Texture newTexture = createTexture(
                        actualData, actualSize, typeToFormat(type), width, height, completeFilepath, type);

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

            while (verticesSize-- > 0) {
                vertices.push_back(stream.read<Vertex>());
            }

            while (indicesSize-- > 0) {
                indices.push_back(stream.read<uint32_t>());
            }

            meshes.push_back(std::make_unique<MeshImpl>(createVerticesBuffer(vertices), createIndicesBuffer(indices), std::move(materials)));
        }

        return std::make_shared<ModelImpl>(std::move(meshes));
    }

    Texture Engine::importTexture(const std::string& filename, TextureType type) {
        COFFEE_ASSERT(backendImpl_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");

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
            rawBytes, static_cast<size_t>(width) * height * 4, Format::R8G8B8A8SRGB, width, height, absoluteFilePath, type);
        materialMap.emplace(absoluteFilePath, loadedTexture);

        stbi_image_free(rawBytes);
        return loadedTexture;
    }

    void Engine::copyBuffer(Buffer& dstBuffer, const Buffer& srcBuffer) {
        COFFEE_ASSERT(backendImpl_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");
        backendImpl_->copyBuffer(dstBuffer, srcBuffer);
    }

    void Engine::copyBufferToImage(Image& dstImage, const Buffer& srcBuffer) {
        COFFEE_ASSERT(backendImpl_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");
        backendImpl_->copyBufferToImage(dstImage, srcBuffer);
    }

    void Engine::waitDeviceIdle() {
        COFFEE_ASSERT(backendImpl_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");
        backendImpl_->waitDevice();
    }

    std::string_view Engine::getClipboard() noexcept {
        COFFEE_ASSERT(backendImpl_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");
        return std::string_view { glfwGetClipboardString(nullptr) };
    }

    void Engine::setClipboard(const std::string& clipboard) noexcept {
        COFFEE_ASSERT(backendImpl_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");
        glfwSetClipboardString(nullptr, clipboard.data());
    }

    void Engine::addMonitorConnectedCallback(const std::string& name, const std::function<void(const MonitorImpl&)>& callback) {
        COFFEE_ASSERT(backendImpl_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");

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

    void Engine::removeMonitorConnectedCallback(const std::string& name) {
        COFFEE_ASSERT(backendImpl_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");

        std::scoped_lock<std::mutex> lock { monitorConnectedMutex_ };
        monitorConnectedCallbacks_.erase(name);
    }

    void Engine::addMonitorDisconnectedCallback(const std::string& name, const std::function<void(const MonitorImpl&)>& callback) {
        COFFEE_ASSERT(backendImpl_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");

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

    void Engine::removeMonitorDisconnectedCallback(const std::string& name) {
        COFFEE_ASSERT(backendImpl_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");

        std::scoped_lock<std::mutex> lock { monitorDisconnectedMutex_ };
        monitorDisconnectedCallbacks_.erase(name);
    }

    Window Engine::Factory::createWindow(WindowSettings settings, const std::string& windowName) {
        COFFEE_ASSERT(backendImpl_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");
        return backendImpl_->createWindow(settings, windowName);
    }

    Buffer Engine::Factory::createBuffer(const BufferConfiguration& configuration) {
        COFFEE_ASSERT(backendImpl_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");
        return backendImpl_->createBuffer(configuration);
    }

    Image Engine::Factory::createImage(const ImageConfiguration& configuration) {
        COFFEE_ASSERT(backendImpl_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");
        return backendImpl_->createImage(configuration);
    }

    Sampler Engine::Factory::createSampler(const SamplerConfiguration& configuration) {
        COFFEE_ASSERT(backendImpl_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");
        return backendImpl_->createSampler(configuration);
    }

    Shader Engine::Factory::createShader(const std::string& fileName, ShaderStage stage, const std::string& entrypoint) {
        COFFEE_ASSERT(backendImpl_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");
        return backendImpl_->createShader(fileName, stage, entrypoint);
    }

    Shader Engine::Factory::createShader(const std::vector<uint8_t>& bytes, ShaderStage stage, const std::string& entrypoint) {
        COFFEE_ASSERT(backendImpl_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");
        return backendImpl_->createShader(bytes, stage, entrypoint);
    }

    DescriptorLayout Engine::Factory::createDescriptorLayout(const std::map<uint32_t, DescriptorBindingInfo>& bindings) {
        COFFEE_ASSERT(backendImpl_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");
        return backendImpl_->createDescriptorLayout(bindings);
    }

    DescriptorSet Engine::Factory::createDescriptorSet(const DescriptorWriter& writer) {
        COFFEE_ASSERT(backendImpl_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");
        return backendImpl_->createDescriptorSet(writer);
    }

    RenderPass Engine::Factory::createRenderPass(const RenderPassConfiguration& configuration) {
        COFFEE_ASSERT(backendImpl_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");
        return backendImpl_->createRenderPass(nullptr, configuration);
    }

    Pipeline Engine::Factory::createPipeline(
        const RenderPass& renderPass,
        const std::vector<DescriptorLayout>& descriptorLayouts,
        const std::vector<Shader>& shaderPrograms,
        const PipelineConfiguration& configuration
    ) {
        COFFEE_ASSERT(backendImpl_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");

        [[ maybe_unused ]] constexpr auto verifyDescriptorLayouts = [](const std::vector<DescriptorLayout>& layouts) noexcept -> bool {
            bool result = true;

            for (const auto& layout : layouts) {
                result &= (layout != nullptr);
            }

            return result;
        };

        COFFEE_ASSERT(renderPass != nullptr, "Invalid RenderPass provided.");
        COFFEE_ASSERT(verifyDescriptorLayouts(descriptorLayouts), "Invalid std::vector<DescriptorLayout> provided.");

        return backendImpl_->createPipeline(renderPass, descriptorLayouts, shaderPrograms, configuration);
    }

    Framebuffer Engine::Factory::createFramebuffer(const RenderPass& renderPass, const FramebufferConfiguration& configuration) {
        COFFEE_ASSERT(backendImpl_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");
        COFFEE_ASSERT(renderPass != nullptr, "Invalid RenderPass provided.");

        return backendImpl_->createFramebuffer(renderPass, configuration);
    }

    CommandBuffer Engine::Factory::createCommandBuffer() {
        COFFEE_ASSERT(backendImpl_ != nullptr, "Did you forgot to call coffee::Engine::initialize()?");
        return backendImpl_->createCommandBuffer();
    }

    void Engine::createNullTexture() {
        defaultTexture_ = createTexture(
            reinterpret_cast<const uint8_t*>("\255\255\255\255"), 4, Format::R8G8B8A8SRGB, 1U, 1U, "null", TextureType::None);
    }

    Texture Engine::createTexture(
        const uint8_t* rawBytes,
        size_t bufferSize,
        Format format,
        uint32_t width,
        uint32_t height,
        const std::string& filePath,
        TextureType type
    ) {
        COFFEE_ASSERT(rawBytes != nullptr, "rawBytes was nullptr.");

        ImageConfiguration textureConfiguration {};
        textureConfiguration.type = ImageType::TwoDimensional;
        textureConfiguration.format = format;
        textureConfiguration.extent.width = width;
        textureConfiguration.extent.height = height;
        textureConfiguration.tiling = ImageTiling::Optimal;
        textureConfiguration.usage = ImageUsage::TransferDestination | ImageUsage::Sampled;
        textureConfiguration.initialState = ResourceState::Undefined;
        textureConfiguration.viewType = ImageViewType::TwoDimensional;
        textureConfiguration.aspects = ImageAspect::Color;
        Image textureImage = backendImpl_->createImage(textureConfiguration);

        BufferConfiguration stagingBufferConfiguration {};
        stagingBufferConfiguration.usage = BufferUsage::TransferSource;
        stagingBufferConfiguration.properties = MemoryProperty::HostVisible;
        stagingBufferConfiguration.instanceCount = 1U;
        stagingBufferConfiguration.instanceSize = bufferSize;
        Buffer stagingBuffer = backendImpl_->createBuffer(stagingBufferConfiguration);

        stagingBuffer->write(rawBytes, bufferSize);
        stagingBuffer->flush();

        backendImpl_->copyBufferToImage(textureImage, stagingBuffer);

        return std::make_shared<TextureImpl>(std::move(textureImage), filePath, type);
    }

    Buffer Engine::createVerticesBuffer(const std::vector<Vertex>& vertices) {
        BufferConfiguration stagingBufferConfiguration {};
        stagingBufferConfiguration.usage = BufferUsage::TransferSource;
        stagingBufferConfiguration.properties = MemoryProperty::HostVisible;
        stagingBufferConfiguration.instanceCount = vertices.size();
        stagingBufferConfiguration.instanceSize = sizeof(Vertex);
        Buffer stagingVerticesBuffer = backendImpl_->createBuffer(stagingBufferConfiguration);

        stagingVerticesBuffer->write(vertices.data(), vertices.size() * sizeof(Vertex));
        stagingVerticesBuffer->flush();

        BufferConfiguration verticesBufferConfiguration{};
        verticesBufferConfiguration.properties = MemoryProperty::DeviceLocal;
        verticesBufferConfiguration.usage = BufferUsage::Vertex | BufferUsage::TransferDestination;
        verticesBufferConfiguration.instanceCount = vertices.size();
        verticesBufferConfiguration.instanceSize = sizeof(Vertex);
        Buffer verticesBuffer = backendImpl_->createBuffer(verticesBufferConfiguration);

        backendImpl_->copyBuffer(verticesBuffer, stagingVerticesBuffer);
        return verticesBuffer;
    }

    Buffer Engine::createIndicesBuffer(const std::vector<uint32_t>& indices) {
        if (indices.empty()) {
            return nullptr;
        }

        BufferConfiguration stagingBufferConfiguration {};
        stagingBufferConfiguration.usage = BufferUsage::TransferSource;
        stagingBufferConfiguration.properties = MemoryProperty::HostVisible;
        stagingBufferConfiguration.instanceCount = indices.size();
        stagingBufferConfiguration.instanceSize = sizeof(uint32_t);
        Buffer stagingIndicesBuffer = backendImpl_->createBuffer(stagingBufferConfiguration);

        stagingIndicesBuffer->write(indices.data(), indices.size() * sizeof(uint32_t));
        stagingIndicesBuffer->flush();

        BufferConfiguration verticesBufferConfiguration{};
        verticesBufferConfiguration.properties = MemoryProperty::DeviceLocal;
        verticesBufferConfiguration.usage = BufferUsage::Index | BufferUsage::TransferDestination;
        verticesBufferConfiguration.instanceCount = indices.size();
        verticesBufferConfiguration.instanceSize = sizeof(uint32_t);
        Buffer indicesBuffer = backendImpl_->createBuffer(verticesBufferConfiguration);

        backendImpl_->copyBuffer(indicesBuffer, stagingIndicesBuffer);
        return indicesBuffer;
    }

    Format Engine::typeToFormat(TextureType type) noexcept {
        switch (type) {
            default:
                return Format::R8UNorm;
            case TextureType::Diffuse:
            case TextureType::Emissive:
                return Format::R8G8B8A8SRGB;
            case TextureType::Normals:
                return Format::R8G8B8A8UNorm;
        }
    }

}