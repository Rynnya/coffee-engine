#ifndef COFFEE_AUDIO_DEVICE
#define COFFEE_AUDIO_DEVICE

#include <coffee/utils/non_moveable.hpp>

#include <AL/alc.h>

#include <memory>
#include <string>
#include <vector>

namespace coffee {

namespace audio {

    class Device;
    using DevicePtr = std::shared_ptr<const Device>;

    class Device : NonMoveable {
    public:
        ~Device() noexcept;

        // Calling this function for the first time will initialize OpenAL library
        static DevicePtr create();

        // This function will return all devices available ONLY if ALC_ENUMERATION_EXT is supported
        // create() must be called at least once for this function to return valid array
        // Otherwise array will be empty
        inline static const std::vector<DevicePtr>& devices() noexcept { return devices_; }

        // Must be called before using any other types of commands on this class
        // Otherwise ContextException will be thrown
        void applyContext() const noexcept;

        inline const std::string& name() const noexcept { return deviceName_; }

    private:
        Device(ALCdevice* device, const char* deviceName);

        static void initialize();

        ALCdevice* device_;
        ALCcontext* context_;

        std::string deviceName_;

        inline static DevicePtr primaryDevice_ = nullptr;
        inline static std::vector<DevicePtr> devices_ {};
    };

} // namespace audio

} // namespace coffee

#endif
