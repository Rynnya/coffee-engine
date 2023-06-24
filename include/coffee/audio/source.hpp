#ifndef COFFEE_AUDIO_SOURCE
#define COFFEE_AUDIO_SOURCE

#include <coffee/audio/buffer.hpp>
#include <coffee/audio/properties.hpp>

#include <memory>
#include <vector>

namespace coffee {

    namespace audio {

        class Source : NonCopyable {
        public:
            ~Source() noexcept;

            Source(Source&& other) noexcept;
            Source& operator=(Source&& other) noexcept;

            static Source create();

            void play();
            void pause();
            void stop();
            void rewind();

            PitchProperty pitch;
            GainProperty gain;
            MinGainProperty minGain;
            MaxGainProperty maxGain;
            MaxDistanceProperty maxDistance;
            RollOffFactorProperty rollOffFactor;
            ConeOuterGainProperty coneOuterGain;
            ConeInnerAngleProperty coneInnerAngle;
            ConeOuterAngleProperty coneOuterAngle;
            ReferenceDistanceProperty referenceDistance;
            PositionProperty position;
            VelocityProperty velocity;
            DirectionProperty direction;
            SourceStateProperty sourceState;
            SourceRelativeProperty sourceRelative;
            LoopingProperty looping;

        private:
            Source();

            void initialize();
            void validate() const;

            ALuint sourceHandle_ = AL_INVALID;
        };

    } // namespace audio

} // namespace coffee

#endif
