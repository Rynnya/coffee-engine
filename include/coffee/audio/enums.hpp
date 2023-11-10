#ifndef COFFEE_AUDIO_ENUMS
#define COFFEE_AUDIO_ENUMS

#include <AL/al.h>

#include <cstdint>

namespace coffee { namespace audio {

    enum class SourceValueType : uint32_t {
        Pitch = AL_PITCH,
        Gain = AL_GAIN,
        MinGain = AL_MIN_GAIN,
        MaxGain = AL_MAX_GAIN,
        ConeGain = AL_CONE_OUTER_GAIN,
        InnerAngle = AL_CONE_INNER_ANGLE,
        OuterAngle = AL_CONE_OUTER_ANGLE,
        MaxDistance = AL_MAX_DISTANCE,
        ReferenceDistance = AL_REFERENCE_DISTANCE,
        RollOffFactor = AL_ROLLOFF_FACTOR
    };

    enum class SourceState : uint32_t { Initial = AL_INITIAL, Playing = AL_PLAYING, Paused = AL_PAUSED, Stopped = AL_STOPPED };

    enum class AudioFormat : uint32_t {
        None = 0,
        Mono8Bit = AL_FORMAT_MONO8,
        Mono16Bit = AL_FORMAT_MONO16,
        Stereo8Bit = AL_FORMAT_STEREO8,
        Stereo16Bit = AL_FORMAT_STEREO16
    };

}} // namespace coffee::audio

#endif
