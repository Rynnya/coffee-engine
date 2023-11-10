#ifndef COFFEE_AUDIO_PROPERTIES
#define COFFEE_AUDIO_PROPERTIES

#include <coffee/audio/enums.hpp>
#include <coffee/interfaces/properties.hpp>
#include <coffee/utils/non_copyable.hpp>

#include <AL/al.h>
#include <glm/vec3.hpp>

#define DEFINE_FLOAT_AUDIO_PROPERTY(NAME, PROPERTY_NAME)                                                                                             \
    namespace properties {                                                                                                                           \
        class NAME##Control : NonCopyable {                                                                                                          \
        public:                                                                                                                                      \
            inline NAME##Control() noexcept = default;                                                                                               \
                                                                                                                                                     \
            inline NAME##Control(ALuint source) noexcept : sourceHandle { source }                                                                   \
            {                                                                                                                                        \
                alGetSourcef(sourceHandle, PROPERTY_NAME, &value);                                                                                   \
            }                                                                                                                                        \
                                                                                                                                                     \
            inline const float& get() const noexcept                                                                                                 \
            {                                                                                                                                        \
                return value;                                                                                                                        \
            }                                                                                                                                        \
                                                                                                                                                     \
            inline const float& set(const float& newValue) noexcept                                                                                  \
            {                                                                                                                                        \
                alSourcef(sourceHandle, PROPERTY_NAME, newValue);                                                                                    \
                alGetSourcef(sourceHandle, PROPERTY_NAME, &value);                                                                                   \
                return value;                                                                                                                        \
            }                                                                                                                                        \
                                                                                                                                                     \
        private:                                                                                                                                     \
            ALuint sourceHandle = AL_INVALID;                                                                                                        \
            float value = 0.0f;                                                                                                                      \
        };                                                                                                                                           \
    }                                                                                                                                                \
                                                                                                                                                     \
    using NAME##Property = coffee::PropertyImpl<float, properties::NAME##Control>;

#define DEFINE_VEC3_AUDIO_PROPERTY(NAME, PROPERTY_NAME)                                                                                              \
    namespace properties {                                                                                                                           \
        class NAME##Control : NonCopyable {                                                                                                          \
        public:                                                                                                                                      \
            inline NAME##Control() noexcept = default;                                                                                               \
                                                                                                                                                     \
            inline NAME##Control(ALuint source) noexcept : sourceHandle { source }                                                                   \
            {                                                                                                                                        \
                alGetSourcefv(sourceHandle, PROPERTY_NAME, reinterpret_cast<float*>(&value));                                                        \
            }                                                                                                                                        \
                                                                                                                                                     \
            inline const glm::vec3& get() const noexcept                                                                                             \
            {                                                                                                                                        \
                return value;                                                                                                                        \
            }                                                                                                                                        \
                                                                                                                                                     \
            inline const glm::vec3& set(const glm::vec3& newValue) noexcept                                                                          \
            {                                                                                                                                        \
                alSourcefv(sourceHandle, PROPERTY_NAME, reinterpret_cast<const float*>(&newValue));                                                  \
                alGetSourcefv(sourceHandle, PROPERTY_NAME, reinterpret_cast<float*>(&value));                                                        \
                return value;                                                                                                                        \
            }                                                                                                                                        \
                                                                                                                                                     \
        private:                                                                                                                                     \
            ALuint sourceHandle = AL_INVALID;                                                                                                        \
            glm::vec3 value { 0.0f };                                                                                                                \
        };                                                                                                                                           \
    }                                                                                                                                                \
                                                                                                                                                     \
    using NAME##Property = coffee::PropertyImpl<glm::vec3, properties::NAME##Control>;

#define DEFINE_CUSTOM_AUDIO_PROPERTY(NAME, PROPERTY_NAME, TYPE)                                                                                      \
    namespace properties {                                                                                                                           \
        class NAME##Control : NonCopyable {                                                                                                          \
        public:                                                                                                                                      \
            inline NAME##Control() noexcept = default;                                                                                               \
                                                                                                                                                     \
            inline NAME##Control(ALuint source) noexcept : sourceHandle { source }                                                                   \
            {                                                                                                                                        \
                alGetSourcei(sourceHandle, PROPERTY_NAME, &actualValue);                                                                             \
            }                                                                                                                                        \
                                                                                                                                                     \
            inline const TYPE& get() const noexcept                                                                                                  \
            {                                                                                                                                        \
                return reinterpret_cast<const TYPE&>(actualValue);                                                                                   \
            }                                                                                                                                        \
                                                                                                                                                     \
            inline const TYPE& set(const TYPE& newValue) noexcept                                                                                    \
            {                                                                                                                                        \
                bufferValue = static_cast<ALint>(newValue);                                                                                          \
                alSourcei(sourceHandle, PROPERTY_NAME, bufferValue);                                                                                 \
                alGetSourcei(sourceHandle, PROPERTY_NAME, &actualValue);                                                                             \
                return reinterpret_cast<const TYPE&>(actualValue);                                                                                   \
            }                                                                                                                                        \
                                                                                                                                                     \
        private:                                                                                                                                     \
            ALuint sourceHandle = AL_INVALID;                                                                                                        \
            ALint bufferValue = 0;                                                                                                                   \
            ALint actualValue = 0;                                                                                                                   \
        };                                                                                                                                           \
    }                                                                                                                                                \
                                                                                                                                                     \
    using NAME##Property = coffee::PropertyImpl<TYPE, properties::NAME##Control>;

namespace coffee { namespace audio {

        DEFINE_FLOAT_AUDIO_PROPERTY(Pitch, AL_PITCH);
        DEFINE_FLOAT_AUDIO_PROPERTY(Gain, AL_GAIN);
        DEFINE_FLOAT_AUDIO_PROPERTY(MinGain, AL_MIN_GAIN);
        DEFINE_FLOAT_AUDIO_PROPERTY(MaxGain, AL_MAX_GAIN);
        DEFINE_FLOAT_AUDIO_PROPERTY(MaxDistance, AL_MAX_DISTANCE);
        DEFINE_FLOAT_AUDIO_PROPERTY(RollOffFactor, AL_ROLLOFF_FACTOR);
        DEFINE_FLOAT_AUDIO_PROPERTY(ConeOuterGain, AL_CONE_OUTER_GAIN);
        DEFINE_FLOAT_AUDIO_PROPERTY(ConeInnerAngle, AL_CONE_INNER_ANGLE);
        DEFINE_FLOAT_AUDIO_PROPERTY(ConeOuterAngle, AL_CONE_OUTER_ANGLE);
        DEFINE_FLOAT_AUDIO_PROPERTY(ReferenceDistance, AL_REFERENCE_DISTANCE);
        DEFINE_VEC3_AUDIO_PROPERTY(Position, AL_POSITION);
        DEFINE_VEC3_AUDIO_PROPERTY(Velocity, AL_VELOCITY);
        DEFINE_VEC3_AUDIO_PROPERTY(Direction, AL_DIRECTION);
        DEFINE_CUSTOM_AUDIO_PROPERTY(SourceRelative, AL_SOURCE_RELATIVE, bool);
        DEFINE_CUSTOM_AUDIO_PROPERTY(Looping, AL_LOOPING, bool);

        namespace properties {

            class SourceStateControl : NonCopyable {
            public:
                inline SourceStateControl() noexcept = default;

                inline SourceStateControl(ALuint source) noexcept : sourceHandle { source }
                {
                    alGetSourcei(sourceHandle, AL_SOURCE_STATE, &actualValue);
                }

                inline const SourceState& get() const noexcept
                {
                    alGetSourcei(sourceHandle, AL_SOURCE_STATE, &actualValue);
                    return reinterpret_cast<const SourceState&>(actualValue);
                }

            private:
                ALuint sourceHandle = AL_INVALID;
                mutable ALint actualValue = 0;
            };

        } // namespace properties

        using SourceStateProperty = coffee::PropertyImpl<SourceState, properties::SourceStateControl>;

}} // namespace coffee::audio

#undef DEFINE_FLOAT_AUDIO_PROPERTY
#undef DEFINE_VEC3_AUDIO_PROPERTY
#undef DEFINE_CUSTOM_AUDIO_PROPERTY

#endif
