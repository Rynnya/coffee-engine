#include <coffee/audio/source.hpp>

#include <coffee/audio/exceptions.hpp>

#include <AL/alc.h>

namespace coffee {

    namespace audio {

        Source::Source()
        {
            validate();

            alGenSources(1, &sourceHandle_);

            if (sourceHandle_ == AL_INVALID) {
                throw AudioException { AudioException::Type::OutOfMemory, "Ran out of memory" };
            }

            initialize();
        }

        Source::~Source() noexcept
        {
            if (sourceHandle_ != AL_INVALID) {
                alDeleteSources(1, &sourceHandle_);
            }
        }

        Source::Source(Source&& other) noexcept
        {
            if (sourceHandle_ != AL_INVALID) {
                alDeleteSources(1, &sourceHandle_);
            }

            sourceHandle_ = std::exchange(other.sourceHandle_, AL_INVALID);

            initialize();
        }

        Source& Source::operator=(Source&& other) noexcept
        {
            if (this == &other) {
                return *this;
            }

            if (sourceHandle_ != AL_INVALID) {
                alDeleteSources(1, &sourceHandle_);
            }

            sourceHandle_ = std::exchange(other.sourceHandle_, AL_INVALID);

            initialize();

            return *this;
        }

        Source Source::create() { return Source {}; }

        void Source::play()
        {
            validate();

            alSourcePlay(sourceHandle_);
        }

        void Source::pause()
        {
            validate();

            alSourcePause(sourceHandle_);
        }

        void Source::stop()
        {
            validate();

            alSourceStop(sourceHandle_);
        }

        void Source::rewind()
        {
            validate();

            alSourceRewind(sourceHandle_);
        }

        void Source::initialize()
        {
            pitch = PitchProperty { sourceHandle_ };
            gain = GainProperty { sourceHandle_ };
            minGain = MinGainProperty { sourceHandle_ };
            maxGain = MaxGainProperty { sourceHandle_ };
            maxDistance = MaxDistanceProperty { sourceHandle_ };
            rollOffFactor = RollOffFactorProperty { sourceHandle_ };
            coneOuterGain = ConeOuterGainProperty { sourceHandle_ };
            coneInnerAngle = ConeInnerAngleProperty { sourceHandle_ };
            coneOuterAngle = ConeOuterAngleProperty { sourceHandle_ };
            referenceDistance = ReferenceDistanceProperty { sourceHandle_ };
            position = PositionProperty { sourceHandle_ };
            velocity = VelocityProperty { sourceHandle_ };
            direction = DirectionProperty { sourceHandle_ };
            sourceState = SourceStateProperty { sourceHandle_ };
            sourceRelative = SourceRelativeProperty { sourceHandle_ };
            looping = LoopingProperty { sourceHandle_ };
        }

        void Source::validate() const
        {
            if (alcGetCurrentContext() == nullptr) {
                throw AudioException { AudioException::Type::ContextFailure, "No context was bound" };
            }

            alGetError();
        }

    } // namespace audio

} // namespace coffee