#ifndef COFFEE_AUDIO_BUFFER
#define COFFEE_AUDIO_BUFFER

#include <coffee/utils/non_copyable.hpp>

#include <AL/al.h>

#include <vector>

namespace coffee {

    namespace audio {

        class Buffer : NonCopyable {
        public:
            ~Buffer() noexcept;

            static Buffer create();
            static std::vector<Buffer> create(uint32_t size);

            void upload();

            private:

        };

    }

}

#endif
