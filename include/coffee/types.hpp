#ifndef COFFEE_TYPES
#define COFFEE_TYPES

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace coffee {

    class AbstractBuffer;
    class AbstractImage;
    class AbstractSampler;

    using Buffer = std::shared_ptr<AbstractBuffer>;
    using Image = std::shared_ptr<AbstractImage>;
    using Sampler = std::shared_ptr<AbstractSampler>;

    enum class BackendAPI : uint32_t {
        // Available everywhere
        Vulkan = 0,
        // Will only work if OS is Windows, always present for backward-compatability
        D3D12 = 1
    };

    enum class CursorState : uint32_t {
        // Cursor visible and not bounded to window
        Visible = 0,
        // Cursor not visible, but still not bounded to window
        Hidden = 1,
        // Cursor not visible and bounded to window, meaning it can expand it positions up to double max
        Disabled = 2
    };

    enum class PresentMode : uint32_t {
        // Default present mode, always supported
        FIFO = 0,
        // No synchronization mode, if Mailbox supported by implementation and GPU, it will be used instead of Immediate
        Immediate = 1
    };

    enum class ShaderStage : uint32_t {
        None = 0,
        Vertex = 1 << 0,
        Geometry = 1 << 1,
        TessellationControl = 1 << 2,
        TessellationEvaluation = 1 << 3,
        Fragment = 1 << 4,
        Compute = 1 << 5,
        All = 1 << 6
    };

    enum class BufferUsage : uint32_t {
        // Specify this flag when you need to store something from CPU side
        TransferSource = 1 << 0,
        // Specify this flag when you need to store something on the GPU side
        TransferDestination = 1 << 1,
        Uniform = 1 << 2,
        Storage = 1 << 3,
        Vertex = 1 << 4,
        Index = 1 << 5,
        Indirect = 1 << 6
    };

    enum class MemoryProperty : uint32_t {
        // Specify this flag if you wanna resource to be accessable only from GPU
        DeviceLocal = 1 << 0,
        // Specify this flag if you wanna resource to be accessable from CPU
        HostVisible = 1 << 1,
        // Specify this flag if you wanna don't care about flush() and invalidate() calls
        HostCoherent = 1 << 2
    };

    enum class ImageType : uint32_t {
        OneDimensional = 0,
        TwoDimensional = 1,
        ThreeDimensional = 2
    };

    enum class ImageViewType : uint32_t {
        OneDimensional = 0,
        TwoDimensional = 1,
        ThreeDimensional = 2,
        Cube = 3,
        OneDimensionalArray = 4,
        TwoDimensionalArray = 5
    };

    enum class ImageUsage : uint32_t {
        None = 0,
        // Specify this flag when you need to store something from CPU side
        TransferSource = 1 << 0,
        // Specify this flag when you need to store something on the GPU side
        TransferDestination = 1 << 1,
        Sampled = 1 << 2,
        Storage = 1 << 3,
        ColorAttachment = 1 << 4,
        DepthStencilAttachment = 1 << 5,
        TransientAttachment = 1 << 6,
        InputAttachment = 1 << 7
    };

    enum class ImageAspect : uint32_t {
        Color = 1 << 0,
        Depth = 1 << 1,
        Stencil = 1 << 2,
        Metadata = 1 << 3
    };

    enum class ImageTiling : uint32_t {
        Optimal = 0,
        Linear = 1
    };

    enum class TextureType : uint32_t {
        None = 0,
        Diffuse = 1 << 0,
        Specular = 1 << 1,
        Normals = 1 << 2,
        Emissive = 1 << 3,
        Roughness = 1 << 4,
        Metallic = 1 << 5,
        AmbientOcclusion = 1 << 6
    };

    enum class SamplerFilter : uint32_t {
        // Also knows as Point
        Nearest = 0,
        Linear = 1
    };

    enum class Format : uint32_t {
        Undefined = 0,
        R8UNorm = 1,
        R8SNorm = 2,
        R8UScaled = 3,
        R8SScaled = 4,
        R8UInt = 5,
        R8SInt = 6,
        R8SRGB = 7,
        R8G8UNorm = 8,
        R8G8SNorm = 9,
        R8G8UScaled = 10,
        R8G8SScaled = 11,
        R8G8UInt = 12,
        R8G8SInt = 13,
        R8G8SRGB = 14,
        R8G8B8UNorm = 15,
        R8G8B8SNorm = 16,
        R8G8B8UScaled = 17,
        R8G8B8SScaled = 18,
        R8G8B8UInt = 19,
        R8G8B8SInt = 20,
        R8G8B8SRGB = 21,
        B8G8R8UNorm = 22,
        B8G8R8SNorm = 23,
        B8G8R8UScaled = 24,
        B8G8R8SScaled = 25,
        B8G8R8UInt = 26,
        B8G8R8SInt = 27,
        B8G8R8SRGB = 28,
        R8G8B8A8UNorm = 29,
        R8G8B8A8SNorm = 30,
        R8G8B8A8UScaled = 31,
        R8G8B8A8SScaled = 32,
        R8G8B8A8UInt = 33,
        R8G8B8A8SInt = 34,
        R8G8B8A8SRGB = 35,
        B8G8R8A8UNorm = 36,
        B8G8R8A8SNorm = 37,
        B8G8R8A8UScaled = 38,
        B8G8R8A8SScaled = 39,
        B8G8R8A8UInt = 40,
        B8G8R8A8SInt = 41,
        B8G8R8A8SRGB = 42,
        R16UNorm = 43,
        R16SNorm = 44,
        R16UScaled = 45,
        R16SScaled = 46,
        R16UInt = 47,
        R16SInt = 48,
        R16SFloat = 49,
        R16G16UNorm = 50,
        R16G16SNorm = 51,
        R16G16UScaled = 52,
        R16G16SScaled = 53,
        R16G16UInt = 54,
        R16G16SInt = 55,
        R16G16SFloat = 56,
        R16G16B16UNorm = 57,
        R16G16B16SNorm = 58,
        R16G16B16UScaled = 59,
        R16G16B16SScaled = 60,
        R16G16B16UInt = 61,
        R16G16B16SInt = 62,
        R16G16B16SFloat = 63,
        R16G16B16A16UNorm = 64,
        R16G16B16A16SNorm = 65,
        R16G16B16A16UScaled = 66,
        R16G16B16A16SScaled = 67,
        R16G16B16A16UInt = 68,
        R16G16B16A16SInt = 69,
        R16G16B16A16SFloat = 70,
        R32UInt = 71,
        R32SInt = 72,
        R32SFloat = 73,
        R32G32UInt = 74,
        R32G32SInt = 75,
        R32G32SFloat = 76,
        R32G32B32UInt = 77,
        R32G32B32SInt = 78,
        R32G32B32SFloat = 79,
        R32G32B32A32UInt = 80,
        R32G32B32A32SInt = 81,
        R32G32B32A32SFloat = 82,
        R64UInt = 83,
        R64SInt = 84,
        R64SFloat = 85,
        R64G64UInt = 86,
        R64G64SInt = 87,
        R64G64SFloat = 88,
        R64G64B64UInt = 89,
        R64G64B64SInt = 90,
        R64G64B64SFloat = 91,
        R64G64B64A64UInt = 92,
        R64G64B64A64SInt = 93,
        R64G64B64A64SFloat = 94,
        S8UInt = 95,
        D16UNorm = 96,
        D16UNormS8UInt = 97,
        D24UNormS8UInt = 98,
        D32SFloat = 99,
        D32SFloatS8UInt = 100,
        R11G11B10UFloat = 101
    };

    enum class Topology : uint32_t {
        Point = 0,
        Line = 1,
        Triangle = 2
    };

    enum class BlendFactor : uint32_t {
        Zero = 0,
        One = 1,
        SrcColor = 2,
        OneMinusSrcColor = 3,
        DstColor = 4,
        OneMinusDstColor = 5,
        SrcAlpha = 6,
        OneMinusSrcAlpha = 7,
        DstAlpha = 8,
        OneMinusDstAlpha = 9,
        ConstantColor = 10,
        OneMinusConstantColor = 11,
        ConstantAlpha = 12,
        OneMinusConstantAlpha = 13,
        SrcAlphaSaturate = 14,
        Src1Color = 15,
        OneMinusSrc1Color = 16,
        Src1Alpha = 17,
        OneMinusSrc1Alpha = 18
    };

    enum class BlendOp : uint32_t {
        Add = 0,
        Subtract = 1,
        ReverseSubtract = 2,
        Min = 4,
        Max = 5
    };

    enum class LogicOp : uint32_t {
        Clear = 0,
        And = 1,
        ReverseAnd = 2,
        Copy = 3,
        InvertedAnd = 4,
        NoOp = 5,
        XOR = 6,
        OR = 7,
        NOR = 8,
        Equivalent = 9,
        Invert = 10,
        ReverseOR = 11,
        InvertedCopy = 12,
        InvertedOR = 13,
        NAND = 14,
        Set = 15
    };

    enum class AttachmentLoadOp : uint32_t {
        Load = 0,
        Clear = 1,
        DontCare = 2
    };

    enum class AttachmentStoreOp : uint32_t {
        Store = 0,
        DontCare = 1
    };

    enum class ColorComponent : uint32_t {
        Red = 1 << 0,
        Green = 1 << 1,
        Blue = 1 << 2,
        Alpha = 1 << 3
    };

    enum class SwizzleComponent : uint32_t {
        Identity = 0,
        Zero = 1,
        One = 2,
        Red = 3,
        Green = 4,
        Blue = 5,
        Alpha = 6
    };

    enum class CompareOp : uint32_t {
        Never = 0,
        Less = 1,
        Equal = 2,
        LessEqual = 3,
        Greater = 4,
        NotEqual = 5,
        GreaterEqual = 6,
        Always = 7
    };

    enum class StencilOp : uint32_t {
        Keep = 0,
        Zero = 1,
        Replace = 2,
        IncreaseAndClamp = 3,
        DecreaseAndClamp = 4,
        Invert = 5,
        IncreaseAndWrap = 6,
        DecreaseAndWrap = 7
    };

    enum class CullMode : uint32_t {
        None = 0,
        Front = 1,
        Back = 2
    };

    enum class FrontFace : uint32_t {
        Clockwise = 0,
        CounterClockwise = 1
    };

    enum class PolygonMode : uint32_t {
        Wireframe = 0,
        Solid = 1
    };

    enum class InputRate : uint32_t {
        PerVertex = 0,
        PerInstance = 1
    };

    enum class AddressMode : uint32_t {
        // Will tile texture at every U, V values. For example, with U = 3, texture will repeat 3 times
        Repeat = 0,
        // Same as Repeat, but mirrored
        MirroredRepeat = 1,
        // Will discard anything that outside the range [0.0, 1.0]
        ClampToEdge = 2,
        // Will use sampler logic at anything that outside the range [0.0, 1.0]
        ClampToBorder = 3
    };

    enum class BorderColor : uint32_t {
        // (0, 0, 0, 0)
        TransparentBlack = 0,
        // (0, 0, 0, 1)
        OpaqueBlack = 1,
        // (1, 1, 1, 1)
        OpaqueWhite = 2
    };

    enum class DescriptorType : uint32_t {
        Sampler = 0,
        ImageAndSampler = 1,
        SampledImage = 2,
        StorageImage = 3,
        UniformBuffer = 4,
        StorageBuffer = 5,
        InputAttachment = 6
    };

    enum class ResourceState : uint32_t {
        Undefined = 1 << 0,
        VertexBuffer = 1 << 1,
        UniformBuffer = 1 << 2,
        IndexBuffer = 1 << 3,
        RenderTarget = 1 << 4,
        UnorderedAccess = 1 << 5,
        DepthRead = 1 << 6,
        DepthWrite = 1 << 7,
        ShaderResource = 1 << 8,
        IndirectCommand = 1 << 9,
        CopyDestination = 1 << 10,
        CopySource = 1 << 11,
        Present = 1 << 12
    };

    enum class MessageSeverity : uint32_t {
        Info = 1 << 0,
        Warning = 1 << 1,
        Error = 1 << 2,
        Critical = 1 << 3
    };

    struct Offset2D {
        int32_t x = 0;
        int32_t y = 0;

        constexpr Offset2D operator+(const Offset2D& other);
        constexpr Offset2D operator-(const Offset2D& other);

        constexpr Offset2D& operator+=(const Offset2D& other);
        constexpr Offset2D& operator-=(const Offset2D& other);

    };

    struct Float2D;
    struct Extent3D;

    struct Extent2D {
        uint32_t width = 0U;
        uint32_t height = 0U;

        constexpr Extent2D operator+(const Extent2D& other) const noexcept;
        constexpr Extent2D operator-(const Extent2D& other) const noexcept;

        constexpr Extent2D operator+(const Offset2D& other) const noexcept;
        constexpr Extent2D operator-(const Offset2D& other) const noexcept;

        constexpr Extent2D operator+(const Float2D& other) const noexcept;
        constexpr Extent2D operator-(const Float2D& other) const noexcept;

        constexpr Extent2D& operator+=(const Extent2D& other) noexcept;
        constexpr Extent2D& operator-=(const Extent2D& other) noexcept;

        constexpr Extent2D& operator+=(const Offset2D& other) noexcept;
        constexpr Extent2D& operator-=(const Offset2D& other) noexcept;

        constexpr Extent2D& operator+=(const Float2D& other) noexcept;
        constexpr Extent2D& operator-=(const Float2D& other) noexcept;

        constexpr operator Float2D() const noexcept;
        constexpr operator Extent3D() const noexcept;

    };

    struct Extent3D {
        uint32_t width = 0U;
        uint32_t height = 0U;
        uint32_t depth = 0U;
    };

    struct WorkArea2D {
        Offset2D offset {};
        Extent2D extent {};
    };

    struct Float2D {
        float x = 0U;
        float y = 0U;

        constexpr Float2D operator+(const Float2D& other) const noexcept;
        constexpr Float2D operator-(const Float2D& other) const noexcept;

        constexpr Float2D operator+(const Offset2D& other) const noexcept;
        constexpr Float2D operator-(const Offset2D& other) const noexcept;

        constexpr Float2D operator+(const Extent2D& other) const noexcept;
        constexpr Float2D operator-(const Extent2D& other) const noexcept;

        constexpr Float2D& operator+=(const Float2D& other) noexcept;
        constexpr Float2D& operator-=(const Float2D& other) noexcept;

        constexpr Float2D& operator+=(const Offset2D& other) noexcept;
        constexpr Float2D& operator-=(const Offset2D& other) noexcept;

        constexpr Float2D& operator+=(const Extent2D& other) noexcept;
        constexpr Float2D& operator-=(const Extent2D& other) noexcept;

        constexpr operator Extent2D() const noexcept;

    };

    struct WindowSettings {
        // Leave as 0 to automatic selection
        Extent2D extent {};
        // Window will be hidden when created, so you can do actual things before presenting anything to user
        // Works only if windowed mode is used
        bool hiddenOnStart = true;
        // Defines if window should have borders (not recommended with fullscreen mode)
        bool borderless = false;
        // Defines if window should take full monitor resolution size (not recommended with borderless mode)
        bool fullscreen = false;
        // Allows cursor to have unlimited bounds, which is perfect solution for 3D
        bool cursorDisabled = false;
        // Defines if user input should be accelerated by an OS
        bool rawInput = true;
    };

    struct BufferConfiguration {
        BufferUsage usage = BufferUsage::Uniform;
        MemoryProperty properties = MemoryProperty::HostVisible;
        uint32_t instanceCount = 0U;
        uint32_t instanceSize = 0U;
    };

    struct ImageConfiguration {
        ImageType type = ImageType::OneDimensional;
        Format format = Format::Undefined;
        Extent3D extent {};
        uint32_t mipLevels = 1U;
        uint32_t arrayLayers = 1U;
        // Must be power of 2, otherwise will be round up to nearest power of 2
        uint32_t samples = 1U;
        ImageTiling tiling = ImageTiling::Optimal;
        ImageUsage usage = ImageUsage::None;
        ResourceState initialState = ResourceState::CopyDestination;
        ImageViewType viewType = ImageViewType::OneDimensional;
        SwizzleComponent swizzleRed = SwizzleComponent::Identity;
        SwizzleComponent swizzleGreen = SwizzleComponent::Identity;
        SwizzleComponent swizzleBlue = SwizzleComponent::Identity;
        SwizzleComponent swizzleAlpha = SwizzleComponent::Identity;
        ImageAspect aspects = ImageAspect::Color;
    };

    struct DescriptorBindingInfo {
        DescriptorType type = DescriptorType::UniformBuffer;
        ShaderStage stages = ShaderStage::None;
        uint32_t descriptorCount = 1U;
    };

    struct DescriptorWriteInfo {
        DescriptorType type = DescriptorType::UniformBuffer;
        size_t bufferOffset = 0U;
        size_t bufferSize = 0U;
        Buffer buffer = nullptr;
        ResourceState imageState = ResourceState::ShaderResource;
        Image image = nullptr;
        Sampler sampler = nullptr;
    };

    struct ClearDepthStencilValue {
        float depth = 1.0f;
        uint32_t stencil = 0U;
    };

    using ColorClearValue = std::variant<std::array<float, 4>, std::array<int32_t, 4>, std::array<uint32_t, 4>, std::monostate>;
    using DepthStencilClearValue = std::variant<ClearDepthStencilValue, std::monostate>;

    struct AttachmentConfiguration {
        uint32_t sampleCount = 1U;
        ResourceState initialState = ResourceState::Undefined;
        ResourceState finalState = ResourceState::Present;
        Format format = Format::Undefined;
        AttachmentLoadOp loadOp = AttachmentLoadOp::Load;
        AttachmentStoreOp storeOp = AttachmentStoreOp::Store;
        AttachmentLoadOp stencilLoadOp = AttachmentLoadOp::DontCare;
        AttachmentStoreOp stencilStoreOp = AttachmentStoreOp::DontCare;
        ColorClearValue clearValue = std::monostate {};
        DepthStencilClearValue depthStencilClearValue = std::monostate {};
        // All multisampled images are resolved automatically if resolveImage isn't nullptr
        // You can resolve them manually by leaving this value as nullptr
        // It will be YOUR responsibility to resolve them
        Image resolveImage = nullptr;
    };

    struct RenderPassConfiguration {
        std::vector<AttachmentConfiguration> colorAttachments {};
        std::variant<AttachmentConfiguration, std::monostate> depthStencilAttachment = std::monostate {};
        // By default, render pass automatically sets barriers and dependencies, when parent is provided
        // You can change this behaviour and it will be YOUR responsibility to set barriers and resolve dependencies
        // It's not recommended to mix behaviours as it will lead to undefined behaviours
        bool automaticBarriersRequired = true;
    };

    struct FramebufferConfiguration {
        Extent2D extent {};
        std::vector<Image> colorViews {};
        Image depthStencilView = nullptr;
        Image resolveView = nullptr;
    };

    struct PushConstantsRange {
        ShaderStage shaderStages = ShaderStage::None;
        uint32_t offset = 0U;
        uint32_t size = 0U;
    };

    struct InputAssembly {
        Topology topology = Topology::Triangle;
        // On every implementation only 32-bit indices allowed. Using 16-bit indices is undefined behaviour
        bool primitiveRestartEnable = false;
    };

    struct RasterizerationInfo {
        CullMode cullMode = CullMode::None;
        PolygonMode fillMode = PolygonMode::Solid;
        FrontFace frontFace = FrontFace::Clockwise;
        bool depthBiasEnable = false;
        float depthBiasConstantFactor = 0.0f;
        float depthBiasClamp = 0.0f;
        float depthBiasSlopeFactor = 0.0f;
    };

    struct MultisampleInfo {
        bool sampleRateShading = false;
        float minSampleShading = 1.0f;
        // Must be power of 2, otherwise will be round up to nearest power of 2
        uint32_t sampleCount = 1U;
        bool alphaToCoverage = false;
    };

    struct ColorBlendAttachment {
        bool blendEnable = false;
        BlendFactor srcBlend = BlendFactor::One;
        BlendFactor dstBlend = BlendFactor::Zero;
        BlendOp blendOp = BlendOp::Add;
        BlendFactor alphaSrcBlend = BlendFactor::One;
        BlendFactor alphaDstBlend = BlendFactor::Zero;
        BlendOp alphaBlendOp = BlendOp::Add;
        bool logicOpEnable = false;
        LogicOp logicOp = LogicOp::Copy;
        ColorComponent colorWriteMask = static_cast<ColorComponent>(15U);
        // ^ Same as coffee::ColorComponent::Red | coffee::ColorComponent::Green | coffee::ColorComponent::Blue | coffee::ColorComponent::Alpha
    };

    struct StencilOpState {
        StencilOp failOp = StencilOp::Keep;
        StencilOp passOp = StencilOp::Keep;
        StencilOp depthFailOp = StencilOp::Keep;
        CompareOp compareOp = CompareOp::Always;
        // Only used in Vulkan, in D3D12 ignored
        uint32_t reference = 0U;
    };

    struct DepthStencilInfo {
        bool depthTestEnable = true;
        bool depthWriteEnable = true;
        CompareOp depthCompareOp = CompareOp::LessEqual;
        bool stencilTestEnable = false;
        uint8_t stencilReadMask = 0U;
        uint8_t stencilWriteMask = 0U;
        StencilOpState frontFace {};
        StencilOpState backFace {};
    };

    struct InputElement {
        // Only used in D3D12, in Vulkan name provided inside Shader
        std::string semanticName {};
        // Only used in D3D12, in Vulkan same names are forbidden
        uint32_t semanticIndex = 0U;
        // Defines a input slot inside input assembler
        uint32_t location = 0U;
        // Defines it's type and size
        Format format = Format::Undefined;
        // Offset over others objects (use offsetof() macro to calculate it properly)
        uint32_t offset = 0U;
    };

    struct InputBinding {
        // Binding number
        uint32_t binding = 0U;
        // Must be same as struct size (use sizeof() to calculate it properly)
        uint32_t stride = 0U;
        // Applies to every object inside 'elements'
        InputRate inputRate = InputRate::PerVertex;
        // Look at InputElement struct for better information
        std::vector<InputElement> elements {};
    };

    struct PipelineConfiguration {
        InputAssembly inputAssembly {};
        RasterizerationInfo rasterizationInfo {};
        MultisampleInfo multisampleInfo {};
        ColorBlendAttachment colorBlendAttachment {};
        DepthStencilInfo depthStencilInfo {};
        std::vector<InputBinding> inputBindings {};
        std::vector<PushConstantsRange> ranges {};
    };

    struct SamplerConfiguration {
        SamplerFilter magFilter = SamplerFilter::Nearest;
        SamplerFilter minFilter = SamplerFilter::Nearest;
        SamplerFilter mipmapMode = SamplerFilter::Nearest;
        AddressMode addressModeU = AddressMode::Repeat;
        AddressMode addressModeV = AddressMode::Repeat;
        AddressMode addressModeW = AddressMode::Repeat;
        float mipLodBias = 0.0f;
        bool anisotropyEnable = false;
        // Must be power of 2, otherwise will be round up to nearest power of 2
        // Set this value to huge value to get device max anisotropy
        uint32_t maxAnisotropy = 0U;
        CompareOp compareOp = CompareOp::Never;
        BorderColor borderColor = BorderColor::TransparentBlack;
        float minLod = 0.0f;
        float maxLod = 0.0f;
    };

}

#include <coffee/types.inl>

#endif