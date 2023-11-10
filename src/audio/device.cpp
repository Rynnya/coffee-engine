#include <coffee/audio/device.hpp>

#include <coffee/audio/exceptions.hpp>
#include <coffee/utils/log.hpp>

#include <al.h>
#include <oneapi/tbb/queuing_mutex.h>

namespace coffee {

    static tbb::queuing_mutex initializationMutex {};

    namespace audio {

        Device::Device(ALCdevice* device, const char* deviceName)
        {
            COFFEE_ASSERT(device != nullptr, "Invalid device handle provided");

            device_ = device;
            context_ = alcCreateContext(device_, nullptr);

            if (context_ == nullptr) {
                throw AudioException { AudioException::Type::DeviceCreationFailure, "Failed to create context for device" };
            }

            deviceName_ = deviceName;

            // Remove prefix that OpenAL inserts into every device if it provided
            size_t erasePosition = deviceName_.find("OpenAL Soft on ");
            if (erasePosition != std::string::npos) {
                deviceName_.erase(erasePosition, sizeof("OpenAL Soft on ") - 1);
            }
        }

        Device::~Device() noexcept
        {
            alcMakeContextCurrent(nullptr);
            alcDestroyContext(context_);
            alcCloseDevice(device_);
        }

        DevicePtr Device::create()
        {
            tbb::queuing_mutex::scoped_lock lock { initializationMutex };

            if (primaryDevice_ == nullptr) {
                initialize();
            }

            return primaryDevice_;
        }

        void Device::applyContext() const noexcept { alcMakeContextCurrent(context_); }

        void Device::initialize()
        {
            if (alcIsExtensionPresent(nullptr, "ALC_ENUMERATION_EXT") == AL_TRUE) {
                const char* devices = alcGetString(nullptr, ALC_ALL_DEVICES_SPECIFIER);
                uint32_t index = 0;

                while (devices[index] != '\0' && devices[index + 1] != '\0') {
                    ALCdevice* deviceHandle = alcOpenDevice(devices + index);

                    if (deviceHandle == nullptr) {
                        throw AudioException { AudioException::Type::DeviceCreationFailure, "Failed to create device" };
                    }

                    std::shared_ptr<Device> DevicePtr = std::shared_ptr<Device>(new Device { deviceHandle, devices + index });

                    if (primaryDevice_ == nullptr) {
                        primaryDevice_ = DevicePtr;
                    }

                    devices_.push_back(DevicePtr);
                    index += std::strlen(devices + index) + 1;
                }
            }
            else {
                ALCdevice* primaryDeviceHandle = alcOpenDevice(nullptr);

                if (primaryDeviceHandle == nullptr) {
                    throw AudioException { AudioException::Type::DeviceCreationFailure, "Failed to create primary device" };
                }

                primaryDevice_ = std::shared_ptr<Device>(new Device { primaryDeviceHandle, alcGetString(nullptr, ALC_DEVICE_SPECIFIER) });
                devices_.push_back(primaryDevice_);
            }

            if (primaryDevice_ != nullptr) {
                primaryDevice_->applyContext();
            }
        }

    } // namespace audio

} // namespace coffee