#include <coffee/abstract/shader.hpp>

#include <coffee/utils/log.hpp>

namespace coffee {

    AbstractShader::AbstractShader(ShaderStage stage, const std::string& entrypoint) noexcept
        : stage_ { stage }
        , entrypoint_ { entrypoint.empty() ? "main" : entrypoint }
    {
        [[ maybe_unused ]] constexpr auto verifyStage = [](ShaderStage stageToCheck) -> bool {
            switch (stageToCheck) {
                case ShaderStage::Vertex:
                case ShaderStage::Geometry:
                case ShaderStage::TessellationControl:
                case ShaderStage::TessellationEvaluation:
                case ShaderStage::Fragment:
                case ShaderStage::Compute:
                    return true;
                case ShaderStage::None:
                case ShaderStage::All:
                default:
                    return false;
            }
        };

        COFFEE_ASSERT(
            verifyStage(stage), 
            "Invalid ShaderStage provided. It must be single input, and must be not ShaderStage::None or ShaderStage::All.");
    }

    ShaderStage AbstractShader::getStage() const noexcept {
        return stage_;
    }

    const std::string& AbstractShader::getEntrypointName() const noexcept {
        return entrypoint_;
    }

}