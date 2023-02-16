#include <coffee/abstract/pipeline.hpp>

namespace coffee {

    const std::vector<PushConstants::Range>& AbstractPipeline::getPushConstantRanges(const PushConstants& pushConstants) noexcept {
        return pushConstants.ranges_;
    }

    const std::map<ShaderStage, Shader, std::less<ShaderStage>>& AbstractPipeline::getShaders(const ShaderProgram& shaderProgram) noexcept {
        return shaderProgram.shaders_;
    }

}