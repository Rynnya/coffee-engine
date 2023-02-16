#ifndef COFFEE_ABSTRACT_PIPELINE
#define COFFEE_ABSTRACT_PIPELINE

#include <coffee/abstract/push_constant.hpp>
#include <coffee/abstract/shader_program.hpp>

namespace coffee {

    class AbstractPipeline : NonMoveable {
    public:
        AbstractPipeline() noexcept = default;
        virtual ~AbstractPipeline() noexcept = default;

    protected:
        static const std::vector<PushConstants::Range>& getPushConstantRanges(const PushConstants& pushConstants) noexcept;
        static const std::map<ShaderStage, Shader, std::less<ShaderStage>>& getShaders(const ShaderProgram& shaderProgram) noexcept;
    };

    using Pipeline = std::unique_ptr<AbstractPipeline>;

}

#endif