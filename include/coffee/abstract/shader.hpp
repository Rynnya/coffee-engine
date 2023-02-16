#ifndef COFFEE_ABSTRACT_SHADER
#define COFFEE_ABSTRACT_SHADER

#include <coffee/utils/non_moveable.hpp>
#include <coffee/types.hpp>

namespace coffee {

    class AbstractShader : NonMoveable {
    public:
        AbstractShader(ShaderStage stage, const std::string& entrypoint) noexcept;
        virtual ~AbstractShader() noexcept = default;

        ShaderStage getStage() const noexcept;
        const std::string& getEntrypointName() const noexcept;

    private:
        ShaderStage stage_;
        std::string entrypoint_;
    };

    using Shader = std::unique_ptr<AbstractShader>;

}

#endif