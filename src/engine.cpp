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
#include <stb_image.h>

#include <chrono>
#include <filesystem>
#include <stdexcept>

namespace coffee {

    struct Engine::PImpl {
        GLFWmonitor* monitorHandle = nullptr;
        GLFWwindow* windowHandle = nullptr;

        uint32_t windowWidth = 0;
        uint32_t windowHeight = 0;
        uint32_t framebufferWidth = 0;
        uint32_t framebufferHeight = 0;

        CursorState cursorState = CursorState::Visible;
        bool rawInputEnabled = false;
        bool windowFocused = false;
        bool textModeEnabled = false;

        double xMousePosition = 0.0;
        double yMousePosition = 0.0;

        static void windowResizeCallback(GLFWwindow* window, int width, int height);
        static void windowFocusCallback(GLFWwindow* window, int focused);
        static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
        static void windowEnterCallback(GLFWwindow* window, int entered);
        static void mouseClickCallback(GLFWwindow* window, int button, int action, int mods);
        static void mousePositionCallback(GLFWwindow* window, double xpos, double ypos);
        static void mouseWheelCallback(GLFWwindow* window, double xoffset, double yoffset);
        static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
        static void charCallback(GLFWwindow* window, unsigned int codepoint);
    };

    void Engine::PImpl::windowResizeCallback(GLFWwindow* window, int width, int height) {
        // This callback most likely will be called with framebufferResizeCallback, so we don't do any callbacks or actions here
        coffee::Engine* enginePtr = static_cast<coffee::Engine*>(glfwGetWindowUserPointer(window));
        enginePtr->pImpl_->windowWidth = static_cast<uint32_t>(width);
        enginePtr->pImpl_->windowHeight = static_cast<uint32_t>(height);
    }

    void Engine::PImpl::windowFocusCallback(GLFWwindow* window, int focused) {
        coffee::Engine* enginePtr = static_cast<coffee::Engine*>(glfwGetWindowUserPointer(window));
        enginePtr->pImpl_->windowFocused = static_cast<bool>(focused);

        const WindowFocusEvent focusEvent { static_cast<bool>(focused) };

        for (const auto& [name, callback] : enginePtr->windowFocusCallbacks_) {
            callback(focusEvent);
        }
    }

    void Engine::PImpl::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
        // Discard all callbacks when window is minimized
        if (width == 0 || height == 0) {
            return;
        }

        // !!! Always recreate swap chain, even if size previous size is same as new one, otherwise it will freeze image
        coffee::Engine* enginePtr = static_cast<coffee::Engine*>(glfwGetWindowUserPointer(window));
        enginePtr->pImpl_->framebufferWidth = static_cast<uint32_t>(width);
        enginePtr->pImpl_->framebufferHeight = static_cast<uint32_t>(height);

        enginePtr->backendImpl_->waitDevice();
        enginePtr->backendImpl_->changeFramebufferSize(enginePtr->pImpl_->framebufferWidth, enginePtr->pImpl_->framebufferHeight);

        const ResizeEvent resizeEvent { enginePtr->pImpl_->framebufferWidth, enginePtr->pImpl_->framebufferHeight };

        for (const auto& [name, callback] : enginePtr->windowResizeCallbacks_) {
            callback(resizeEvent);
        }
    }

    void Engine::PImpl::windowEnterCallback(GLFWwindow* window, int entered) {
        coffee::Engine* enginePtr = static_cast<coffee::Engine*>(glfwGetWindowUserPointer(window));
        const WindowEnterEvent enterEvent { static_cast<bool>(entered) };

        for (const auto& [name, callback] : enginePtr->windowEnterCallbacks_) {
            callback(enterEvent);
        }
    }

    void Engine::PImpl::mouseClickCallback(GLFWwindow* window, int button, int action, int mods) {
        coffee::Engine* enginePtr = static_cast<coffee::Engine*>(glfwGetWindowUserPointer(window));
        const MouseClickEvent mouseClickEvent {
            glfwToCoffeeState(action),
            static_cast<MouseButton>(button),
            static_cast<uint32_t>(mods)
        };

        for (const auto& [name, callback] : enginePtr->mouseClickCallbacks_) {
            callback(mouseClickEvent);
        }
    }

    void Engine::PImpl::mousePositionCallback(GLFWwindow* window, double xpos, double ypos) {
        coffee::Engine* enginePtr = static_cast<coffee::Engine*>(glfwGetWindowUserPointer(window));
        const MouseMoveEvent moveEvent { static_cast<float>(xpos), static_cast<float>(ypos) };

        for (const auto& [name, callback] : enginePtr->mousePositionCallbacks_) {
            callback(moveEvent);
        }

        enginePtr->pImpl_->xMousePosition = xpos;
        enginePtr->pImpl_->yMousePosition = ypos;
    }

    void Engine::PImpl::mouseWheelCallback(GLFWwindow* window, double xoffset, double yoffset) {
        coffee::Engine* enginePtr = static_cast<coffee::Engine*>(glfwGetWindowUserPointer(window));
        const MouseWheelEvent wheelEvent { static_cast<float>(xoffset), static_cast<float>(yoffset) };

        for (const auto& [name, callback] : enginePtr->mouseWheelCallbacks_) {
            callback(wheelEvent);
        }
    }

    void Engine::PImpl::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        coffee::Engine* enginePtr = static_cast<coffee::Engine*>(glfwGetWindowUserPointer(window));
        const KeyEvent keyEvent { 
            glfwToCoffeeState(action),
            static_cast<Keys>(key),
            static_cast<uint32_t>(scancode),
            static_cast<uint32_t>(mods)
        };

        for (const auto& [name, callback] : enginePtr->keyCallbacks_) {
            callback(keyEvent);
        }
    }

    void Engine::PImpl::charCallback(GLFWwindow* window, unsigned int codepoint) {
        coffee::Engine* enginePtr = static_cast<coffee::Engine*>(glfwGetWindowUserPointer(window));

        if (enginePtr->pImpl_->textModeEnabled) {
            for (const auto& [name, callback] : enginePtr->charCallbacks_) {
                callback(codepoint);
            }
        }
    }

    Engine::Engine(BackendSelect backend, WindowSettings settings, const std::string& applicationName)
        : currentBackend_ { backend }
        , pImpl_ { new Engine::PImpl {} }
    {
        COFFEE_THROW_IF(glfwInit() != GLFW_TRUE, "Failed to initialize GLFW!");
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        pImpl_->monitorHandle = glfwGetPrimaryMonitor();
        COFFEE_THROW_IF(pImpl_->monitorHandle == nullptr, "Failed to get primary monitor handle!");

        if (settings.fullscreen && settings.borderless) {
            COFFEE_WARNING(
                "It's generally not recommended to use Fullscreen and Borderless mode at the same time. "
                "If you have any issues - try to disable one of those!");
        }

        // Window cannot be hidden when using fullscreen mode
        // Otherwise it will cause a window to be (0, 0)
        if (settings.hiddenOnStart && !settings.fullscreen) {
            glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        }

        if (!settings.fullscreen) {
            glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        }

        if (settings.borderless) {
            glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
        }

        if (settings.width == 0 || settings.height == 0) {
            const GLFWvidmode* videoMode = glfwGetVideoMode(pImpl_->monitorHandle);
            COFFEE_THROW_IF(videoMode == nullptr, "Failed to retreave main video mode of primary monitor!");

            settings.width = static_cast<uint32_t>(settings.fullscreen
                ? videoMode->width
                : videoMode->width - videoMode->width / 2);
            settings.height = static_cast<uint32_t>(settings.fullscreen
                ? videoMode->height
                : videoMode->height - videoMode->height / 2);
        }

        pImpl_->windowHandle = glfwCreateWindow(
            settings.width, 
            settings.height, 
            applicationName.c_str(), 
            settings.fullscreen ? pImpl_->monitorHandle : nullptr, 
            nullptr);
        COFFEE_THROW_IF(pImpl_->windowHandle == nullptr, "Failed to create new GLFW window!");

        glfwSetInputMode(pImpl_->windowHandle, GLFW_STICKY_KEYS, GLFW_TRUE);
        if (settings.cursorDisabled) {
            glfwSetInputMode(pImpl_->windowHandle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            pImpl_->cursorState = CursorState::Disabled;
        }

        if (glfwRawMouseMotionSupported() && settings.rawInput) {
            glfwSetInputMode(pImpl_->windowHandle, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
            pImpl_->rawInputEnabled = true;
        }

        int width {}, height {};
        glfwGetWindowSize(pImpl_->windowHandle, &width, &height);
        pImpl_->windowWidth = static_cast<uint32_t>(width);
        pImpl_->windowHeight = static_cast<uint32_t>(height);

        glfwGetFramebufferSize(pImpl_->windowHandle, &width, &height);
        pImpl_->framebufferWidth = static_cast<uint32_t>(width);
        pImpl_->framebufferHeight = static_cast<uint32_t>(height);

        pImpl_->windowFocused = static_cast<bool>(glfwGetWindowAttrib(pImpl_->windowHandle, GLFW_FOCUSED));

        glfwSetWindowSizeCallback(pImpl_->windowHandle, pImpl_->windowResizeCallback);
        glfwSetWindowFocusCallback(pImpl_->windowHandle, pImpl_->windowFocusCallback);
        glfwSetFramebufferSizeCallback(pImpl_->windowHandle, pImpl_->framebufferResizeCallback);
        glfwSetCursorEnterCallback(pImpl_->windowHandle, pImpl_->windowEnterCallback);
        glfwSetMouseButtonCallback(pImpl_->windowHandle, pImpl_->mouseClickCallback);
        glfwSetCursorPosCallback(pImpl_->windowHandle, pImpl_->mousePositionCallback);
        glfwSetScrollCallback(pImpl_->windowHandle, pImpl_->mouseWheelCallback);
        glfwSetKeyCallback(pImpl_->windowHandle, pImpl_->keyCallback);
        glfwSetCharCallback(pImpl_->windowHandle, pImpl_->charCallback);
        glfwSetWindowUserPointer(pImpl_->windowHandle, this);

        switch (backend) {
            case BackendSelect::Vulkan:
                backendImpl_ = std::make_unique<VulkanBackend>(pImpl_->windowHandle, pImpl_->windowWidth, pImpl_->windowHeight);
                break;
            case BackendSelect::D3D12:
                throw std::runtime_error("D3D12 is not implemented.");
                break;
            default:
                COFFEE_ASSERT(false, "Invalid BackendSelect provided. This should not happen.");
                throw std::runtime_error("Invalid BackendSelect provided.");
                break;
        }

        createNullTexture();

        lastPollTime_ = std::chrono::high_resolution_clock::now();

        COFFEE_INFO("Engine initialized!");
    }

    Engine::~Engine() {
        glfwPollEvents();
        applyRequests();

        waitDeviceIdle();
        loadedTextures_.fill({});
        defaultTexture_ = nullptr;
        backendImpl_ = nullptr;

        glfwDestroyWindow(pImpl_->windowHandle);
        glfwTerminate();

        delete pImpl_;

        COFFEE_INFO("Engine de-initialized!");
    }

    void Engine::pollEvents() {
        glfwPollEvents();
        applyRequests();
    }

    bool Engine::acquireFrame() {
        return backendImpl_->acquireFrame();
    }

    void Engine::sendCommandBuffer(CommandBuffer&& commandBuffer) {
        COFFEE_ASSERT(commandBuffer != nullptr, "Invalid CommandBuffer provided to sendCommandBuffer.");

        backendImpl_->sendCommandBuffer(std::move(commandBuffer));
    }

    void Engine::endFrame() {
        backendImpl_->endFrame();
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

    BackendSelect Engine::getBackendSelect() const noexcept {
        return currentBackend_;
    }

    Format Engine::getColorFormat() const noexcept {
        return backendImpl_->getColorFormat();
    }

    Format Engine::getDepthFormat() const noexcept {
        return backendImpl_->getDepthFormat();
    }

    uint32_t Engine::getCurrentImageIndex() const noexcept {
        return backendImpl_->getCurrentImageIndex();
    }

    const std::vector<Image>& Engine::getPresentImages() const noexcept {
        return backendImpl_->getPresentImages();
    }

    Model Engine::importModel(const std::string& filename) {
        Assimp::Importer importer {};
        uint32_t processFlags =
            aiProcess_Triangulate |
            aiProcess_OptimizeMeshes |
            aiProcess_JoinIdenticalVertices |
            //aiProcess_MakeLeftHanded |
            aiProcess_GenNormals |
            aiProcess_CalcTangentSpace |
            aiProcess_FlipUVs |
            aiProcess_PreTransformVertices;

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
                case aiTextureType_AMBIENT:
                    return { TextureType::Ambient, Format::R8UNorm, STBI_grey };
                case aiTextureType_EMISSIVE:
                    return { TextureType::Emissive, Format::R8G8B8A8SRGB, STBI_rgb_alpha };
                case aiTextureType_HEIGHT:
                    return { TextureType::Height, Format::R8UNorm, STBI_grey };
                case aiTextureType_NORMALS:
                    return { TextureType::Normals, Format::R8G8B8A8UNorm, STBI_rgb_alpha };
                case aiTextureType_DIFFUSE_ROUGHNESS:
                    return { TextureType::Roughness, Format::R8UNorm, STBI_grey };
                case aiTextureType_METALNESS:
                    return { TextureType::Metallic, Format::R8UNorm, STBI_grey };
                case aiTextureType_AMBIENT_OCCLUSION:
                    return { TextureType::AmbientOcclusion, Format::R8UNorm, STBI_grey };
            }
        };

        auto createNativeBuffers = [this](
            const std::vector<Vertex>& vertices,
            const std::vector<uint32_t>& indices
        ) -> std::pair<Buffer, Buffer> {

            Buffer verticesBuffer = nullptr;
            Buffer indicesBuffer = nullptr;

            {
                BufferConfiguration stagingBufferConfiguration{};
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
                verticesBuffer = backendImpl_->createBuffer(verticesBufferConfiguration);

                backendImpl_->copyBuffer(verticesBuffer, stagingVerticesBuffer);
            }

            if (!indices.empty())
            {
                BufferConfiguration stagingBufferConfiguration{};
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
                indicesBuffer = backendImpl_->createBuffer(verticesBufferConfiguration);

                backendImpl_->copyBuffer(indicesBuffer, stagingIndicesBuffer);
            }

            return std::make_pair(verticesBuffer, indicesBuffer);
        };

        auto loadMaterialTextures = [this, &assimpTypeToEngineType, &parentDirectory](
            Materials& materials,
            const aiScene* scene,
            aiMaterial* material,
            aiTextureType textureType
        ) -> void {
            auto&& [engineType, formatType, stbiType] = assimpTypeToEngineType(textureType);
            uint32_t materialTypeIndex = Math::indexOfHighestBit(static_cast<uint32_t>(engineType));
            COFFEE_ASSERT(materialTypeIndex >= 0 && materialTypeIndex <= 8, "Invalid TextureType provided.");

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

        auto loadSubmesh = [this, &assimpTypeToEngineType, &loadMaterialTextures, &createNativeBuffers](
            const aiScene* scene, 
            aiMesh* mesh
        ) -> Mesh {
            std::vector<Vertex> vertices {};
            std::vector<uint32_t> indices {};
            Materials materials { defaultTexture_ };

            for (uint32_t i = 0; i < mesh->mNumVertices; i++) {
                Vertex vertex {};

                vertex.position = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };

                if (mesh->HasNormals()) {
                    vertex.normal = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };
                }

                if (mesh->HasTextureCoords(0)) {
                    vertex.texCoords = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };
                }

                if (mesh->HasTangentsAndBitangents()) {
                    vertex.tangent = { mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z };
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
                loadMaterialTextures(materials, scene, material, aiTextureType_AMBIENT);
                loadMaterialTextures(materials, scene, material, aiTextureType_NORMALS);
                loadMaterialTextures(materials, scene, material, aiTextureType_METALNESS);
                loadMaterialTextures(materials, scene, material, aiTextureType_DIFFUSE_ROUGHNESS);

                aiColor3D diffuseColor { 1.0f, 1.0f, 1.0f };
                materials.modifiers.diffuseColor.r = diffuseColor.r;
                materials.modifiers.diffuseColor.g = diffuseColor.g;
                materials.modifiers.diffuseColor.b = diffuseColor.b;

                aiColor3D specularColor { 0.0f, 0.0f, 0.0f };
                materials.modifiers.specularColor.r = specularColor.r;
                materials.modifiers.specularColor.g = specularColor.g;
                materials.modifiers.specularColor.b = specularColor.b;

                aiColor3D ambientColor { 0.0f, 0.0f, 0.0f };
                if (material->Get(AI_MATKEY_COLOR_AMBIENT, ambientColor) == aiReturn_SUCCESS) {
                    materials.modifiers.ambientColor.r = ambientColor.r;
                    materials.modifiers.ambientColor.g = ambientColor.g;
                    materials.modifiers.ambientColor.b = ambientColor.b;
                    materials.modifiers.ambientColor.a = 1.0f;
                }
                else {
                    materials.modifiers.ambientColor = { 1.0f, 1.0f, 1.0f, 0.02f };
                }

                material->Get(AI_MATKEY_SHININESS, materials.modifiers.shininessExponent);
                material->Get(AI_MATKEY_METALLIC_FACTOR, materials.modifiers.metallicFactor);
                material->Get(AI_MATKEY_ROUGHNESS_FACTOR, materials.modifiers.roughnessFactor);
            }
            
            auto [verticesBuffer, indicesBuffer] = createNativeBuffers(vertices, indices);
            return std::make_unique<MeshImpl>(std::move(verticesBuffer), std::move(indicesBuffer), std::move(materials));
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

    Texture Engine::importTexture(const std::string& filename, TextureType type) {
        uint32_t materialTypeIndex = static_cast<uint32_t>(type) - 1;
        COFFEE_ASSERT(materialTypeIndex >= 0 && materialTypeIndex < 10, "Invalid TextureType provided.");

        std::string absoluteFilePath = std::filesystem::absolute(filename).generic_string();
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

    Buffer Engine::createBuffer(const BufferConfiguration& configuration) {
        return backendImpl_->createBuffer(configuration);
    }

    Image Engine::createImage(const ImageConfiguration& configuration) {
        return backendImpl_->createImage(configuration);
    }

    Sampler Engine::createSampler(const SamplerConfiguration& configuration) {
        return backendImpl_->createSampler(configuration);
    }

    Shader Engine::createShader(const std::string& fileName, ShaderStage stage, const std::string& entrypoint) {
        return backendImpl_->createShader(fileName, stage, entrypoint);
    }

    Shader Engine::createShader(const std::vector<uint8_t>& bytes, ShaderStage stage, const std::string& entrypoint) {
        return backendImpl_->createShader(bytes, stage, entrypoint);
    }

    DescriptorLayout Engine::createDescriptorLayout(const std::map<uint32_t, DescriptorBindingInfo>& bindings) {
        return backendImpl_->createDescriptorLayout(bindings);
    }

    DescriptorSet Engine::createDescriptorSet(const DescriptorWriter& writer) {
        return backendImpl_->createDescriptorSet(writer);
    }

    RenderPass Engine::createRenderPass(const RenderPassConfiguration& configuration) {
        return backendImpl_->createRenderPass(nullptr, configuration);
    }

    RenderPass Engine::createRenderPass(const RenderPass& parent, const RenderPassConfiguration& configuration) {
        COFFEE_ASSERT(parent != nullptr, "Invalid RenderPass provided.");

        return backendImpl_->createRenderPass(parent, configuration);
    }

    Pipeline Engine::createPipeline(
        const RenderPass& renderPass,
        const PushConstants& pushConstants,
        const std::vector<DescriptorLayout>& descriptorLayouts,
        const ShaderProgram& shaderProgram,
        const PipelineConfiguration& configuration
    ) {
        [[ maybe_unused ]] constexpr auto verifyDescriptorLayouts = [](const std::vector<DescriptorLayout>& layouts) noexcept -> bool {
            bool result = true;

            for (const auto& layout : layouts) {
                result &= (layout != nullptr);
            }

            return result;
        };

        COFFEE_ASSERT(renderPass != nullptr, "Invalid RenderPass provided.");
        COFFEE_ASSERT(verifyDescriptorLayouts(descriptorLayouts), "Invalid std::vector<DescriptorLayout> provided.");

        return backendImpl_->createPipeline(renderPass, pushConstants, descriptorLayouts, shaderProgram, configuration);
    }

    Framebuffer Engine::createFramebuffer(const RenderPass& renderPass, const FramebufferConfiguration& configuration) {
        COFFEE_ASSERT(renderPass != nullptr, "Invalid RenderPass provided.");

        return backendImpl_->createFramebuffer(renderPass, configuration);
    }

    CommandBuffer Engine::createCommandBuffer() {
        return backendImpl_->createCommandBuffer();
    }

    void Engine::changePresentMode(PresentMode newMode) {
        addRequest([this, newMode]() {
            while (pImpl_->framebufferWidth == 0 || pImpl_->framebufferHeight == 0) {
                glfwWaitEvents();
            }

            backendImpl_->waitDevice();
            backendImpl_->changePresentMode(pImpl_->framebufferWidth, pImpl_->framebufferHeight, newMode);

            coffee::PresentModeEvent presentModeEvent { newMode };

            for (const auto& [name, callback] : presentModeCallbacks_) {
                callback(presentModeEvent);
            }
        });
    }

    void Engine::copyBuffer(Buffer& dstBuffer, const Buffer& srcBuffer) {
        backendImpl_->copyBuffer(dstBuffer, srcBuffer);
    }

    void Engine::copyBufferToImage(Image& dstImage, const Buffer& srcBuffer) {
        backendImpl_->copyBufferToImage(dstImage, srcBuffer);
    }

    void Engine::waitDeviceIdle() {
        backendImpl_->waitDevice();
    }

    bool Engine::isWindowFocused() const noexcept {
        return pImpl_->windowFocused;
    }

    void Engine::hideWindow() const noexcept {
        glfwHideWindow(pImpl_->windowHandle);
    }

    void Engine::showWindow() const noexcept {
        glfwShowWindow(pImpl_->windowHandle);
    }

    std::string_view Engine::getClipboard() const noexcept {
        return std::string_view { glfwGetClipboardString(pImpl_->windowHandle) };
    }

    void Engine::setClipboard(const std::string& clipboard) {
        addRequest([this, clipboard]() {
            glfwSetClipboardString(pImpl_->windowHandle, clipboard.data());
        });
    }

    uint32_t Engine::getWindowWidth() const noexcept {
        return pImpl_->windowWidth;
    }

    uint32_t Engine::getWindowHeight() const noexcept {
        return pImpl_->windowHeight;
    }

    uint32_t Engine::getFramebufferWidth() const noexcept {
        return pImpl_->framebufferWidth;
    }

    uint32_t Engine::getFramebufferHeight() const noexcept {
        return pImpl_->framebufferHeight;
    }

    float Engine::getMouseX() const noexcept {
        return static_cast<float>(pImpl_->xMousePosition);
    }

    float Engine::getMouseY() const noexcept {
        return static_cast<float>(pImpl_->yMousePosition);
    }

    bool Engine::isButtonPressed(Keys key) const noexcept {
        return glfwGetKey(pImpl_->windowHandle, *key) == GLFW_PRESS;
    }

    bool Engine::isButtonPressed(MouseButton mouseButton) const noexcept {
        return glfwGetMouseButton(pImpl_->windowHandle, *mouseButton) == GLFW_PRESS;
    }

    CursorState Engine::getCursorState() const noexcept {
        return pImpl_->cursorState;
    }

    void Engine::showCursor() {
        addRequest([this]() {
            glfwSetInputMode(pImpl_->windowHandle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            pImpl_->cursorState = CursorState::Visible;
        });
    }

    void Engine::hideCursor() {
        addRequest([this]() {
            glfwSetInputMode(pImpl_->windowHandle, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
            pImpl_->cursorState = CursorState::Hidden;
        });
    }

    void Engine::disableCursor() {
        addRequest([this]() {
            glfwSetInputMode(pImpl_->windowHandle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            pImpl_->cursorState = CursorState::Disabled;
        });
    }

    void Engine::setCursorPosition(float x, float y) const noexcept {
        glfwSetCursorPos(pImpl_->windowHandle, static_cast<double>(x), static_cast<double>(y));
    }

    bool Engine::isTextMode() const noexcept {
        return pImpl_->textModeEnabled;
    }

    void Engine::enableTextMode() {
        addRequest([this]() {
            pImpl_->textModeEnabled = true;
        });
    }

    void Engine::disableTextMode() {
        addRequest([this]() {
            pImpl_->textModeEnabled = false;
        });
    }

    float Engine::getDeltaTime() const noexcept {
        return deltaTime_;
    }

    float Engine::getFrameLimit() const noexcept {
        return framerateLimit_;
    }

    void Engine::setFrameLimit(float newFrameLimit) noexcept {
        addRequest([this, newFrameLimit]() {
            framerateLimit_ = newFrameLimit;
        });
    }

    bool Engine::shouldExit() const noexcept {
        return static_cast<bool>(glfwWindowShouldClose(pImpl_->windowHandle));
    }

    void Engine::addPresentModeCallback(const std::string& name, const std::function<void(const PresentModeEvent)>& callback) {
        addRequest([this, name, callback]() mutable {
            if (callback == nullptr) {
                COFFEE_ERROR("Callback with name '{}' was discarded because it was nullptr", name);
                return;
            }

            if (presentModeCallbacks_.find(name) != presentModeCallbacks_.end()) {
                COFFEE_WARNING("Callback with name '{}' was replaced", name);
            }

            presentModeCallbacks_[name] = callback;
        });
    }

    void Engine::addWindowFocusCallback(const std::string& name, const std::function<void(const WindowFocusEvent&)>& callback) {
        addRequest([this, name, callback]() mutable {
            if (callback == nullptr) {
                COFFEE_ERROR("Callback with name '{}' was discarded because it was nullptr", name);
                return;
            }

            if (windowFocusCallbacks_.find(name) != windowFocusCallbacks_.end()) {
                COFFEE_WARNING("Callback with name '{}' was replaced", name);
            }

            windowFocusCallbacks_[name] = callback;
        });
    }

    void Engine::addWindowResizeCallback(const std::string& name, const std::function<void(const ResizeEvent&)>& callback) {
        addRequest([this, name, callback]() mutable {
            if (callback == nullptr) {
                COFFEE_ERROR("Callback with name '{}' was discarded because it was nullptr", name);
                return;
            }

            if (windowResizeCallbacks_.find(name) != windowResizeCallbacks_.end()) {
                COFFEE_WARNING("Callback with name '{}' was replaced", name);
            }

            windowResizeCallbacks_[name] = callback;
        });
    }

    void Engine::addWindowEnterCallback(const std::string& name, const std::function<void(const WindowEnterEvent&)>& callback) {
        addRequest([this, name, callback]() mutable {
            if (callback == nullptr) {
                COFFEE_ERROR("Callback with name '{}' was discarded because it was nullptr", name);
                return;
            }

            if (windowEnterCallbacks_.find(name) != windowEnterCallbacks_.end()) {
                COFFEE_WARNING("Callback with name '{}' was replaced", name);
            }

            windowEnterCallbacks_[name] = callback;
        });
    }

    void Engine::addMouseClickCallback(const std::string& name, const std::function<void(const MouseClickEvent&)>& callback) {
        addRequest([this, name, callback]() mutable {
            if (callback == nullptr) {
                COFFEE_ERROR("Callback with name '{}' was discarded because it was nullptr", name);
                return;
            }

            if (mouseClickCallbacks_.find(name) != mouseClickCallbacks_.end()) {
                COFFEE_WARNING("Callback with name '{}' was replaced", name);
            }

            mouseClickCallbacks_[name] = callback;
        });
    }

    void Engine::addMousePositionCallback(const std::string& name, const std::function<void(const MouseMoveEvent&)>& callback) {
        addRequest([this, name, callback]() mutable {
            if (callback == nullptr) {
                COFFEE_ERROR("Callback with name '{}' was discarded because it was nullptr", name);
                return;
            }

            if (mousePositionCallbacks_.find(name) != mousePositionCallbacks_.end()) {
                COFFEE_WARNING("Callback with name '{}' was replaced", name);
            }

            mousePositionCallbacks_[name] = callback;
        });
    }

    void Engine::addMouseWheelCallback(const std::string& name, const std::function<void(const MouseWheelEvent&)>& callback) {
        addRequest([this, name, callback]() mutable {
            if (callback == nullptr) {
                COFFEE_ERROR("Callback with name '{}' was discarded because it was nullptr", name);
                return;
            }

            if (mouseWheelCallbacks_.find(name) != mouseWheelCallbacks_.end()) {
                COFFEE_WARNING("Callback with name '{}' was replaced", name);
            }

            mouseWheelCallbacks_[name] = callback;
        });
    }

    void Engine::addKeyCallback(const std::string& name, const std::function<void(const KeyEvent&)>& callback) {
        addRequest([this, name, callback]() mutable {
            if (callback == nullptr) {
                COFFEE_ERROR("Callback with name '{}' was discarded because it was nullptr", name);
                return;
            }

            if (keyCallbacks_.find(name) != keyCallbacks_.end()) {
                COFFEE_WARNING("Callback with name '{}' was replaced", name);
            }

            keyCallbacks_[name] = callback;
        });
    }

    void Engine::addCharCallback(const std::string& name, const std::function<void(char32_t)>& callback) {
        addRequest([this, name, callback]() mutable {
            if (callback == nullptr) {
                COFFEE_ERROR("Callback with name '{}' was discarded because it was nullptr", name);
                return;
            }

            if (charCallbacks_.find(name) != charCallbacks_.end()) {
                COFFEE_WARNING("Callback with name '{}' was replaced", name);
            }

            charCallbacks_[name] = callback;
        });
    }

    void Engine::removePresentModeCallback(const std::string& name) {
        addRequest([this, name]() {
            presentModeCallbacks_.erase(name);
        });
    }

    void Engine::removeWindowFocusCallback(const std::string& name) {
        addRequest([this, name]() {
            windowFocusCallbacks_.erase(name);
        });
    }

    void Engine::removeWindowResizeCallback(const std::string& name) {
        addRequest([this, name]() {
            windowResizeCallbacks_.erase(name);
        });
    }

    void Engine::removeWindowEnterCallback(const std::string& name) {
        addRequest([this, name]() {
            windowEnterCallbacks_.erase(name);
        });
    }

    void Engine::removeMouseClickCallback(const std::string& name) {
        addRequest([this, name]() {
            mouseClickCallbacks_.erase(name);
        });
    }

    void Engine::removeMousePositionCallback(const std::string& name) {
        addRequest([this, name]() {
            mousePositionCallbacks_.erase(name);
        });
    }

    void Engine::removeMouseWheelCallback(const std::string& name) {
        addRequest([this, name]() {
            mouseWheelCallbacks_.erase(name);
        });
    }

    void Engine::removeKeyCallback(const std::string& name) {
        addRequest([this, name]() {
            keyCallbacks_.erase(name);
        });
    }

    void Engine::removeCharCallback(const std::string& name) {
        addRequest([this, name]() {
            charCallbacks_.erase(name);
        });
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
        textureConfiguration.width = width;
        textureConfiguration.height = height;
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

}