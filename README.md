# Coffee Engine

An experimental engine for game development, made by Rynnya with support of `Vulkan Game Engine Dev` [server](https://discord.gg/Mh9ZjUs5KZ)

### The engine currently in deep development process, and it's alpha/beta release might happen at the end of 2023

## Features

Currently engine handles:
- Graphics
- Audio
- Asset Manager
- Dedicated filesystem and model format
- Dedicated compressor (currently not uploaded)

Features that will come later:
- Unit testing
- Physics
- Scenes

## Libraries
| Library name | License | Reason to add | Provided to end-user |
| --- | --- | --- | --- |
| [fmt](https://github.com/fmtlib/fmt) | Modified MIT | logging purposes | :white_check_mark: |
| [glfw](https://github.com/glfw/glfw) | zlib | window manager (wrapped into `graphics::Window` class) | :warning: |
| [glm](https://github.com/g-truc/glm) | MIT | vectors/matrix multiplications | :white_check_mark: |
| [OpenAL Soft](https://github.com/kcat/openal-soft) | LGPL-2.0 (dll required) | audio manager (wrapped into `audio` namespace) | :warning: |
| [BASISU (Basis Universal)](https://github.com/BinomialLLC/basis_universal) | Apache 2.0 | texture super-compression | :warning: |
| [mio](https://github.com/mandreyel/mio) | MIT | file mapping for virtual filesystem | :warning: |
| [VMA](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator) | MIT | Vulkan Memory Allocator | :warning: |
| [volk](https://github.com/zeux/volk) | MIT | meta loader for Vulkan (basically free performance) | :warning: |
| [XXH3](https://github.com/Cyan4973/xxHash) | BSD 2-Clause | hashing function in virtual filesystem | :x: |
| [zstd](https://github.com/facebook/zstd) | BSD | compression algorithm, used in BASISU | :x: |
| [TBB](https://github.com/oneapi-src/oneTBB) | Apache 2.0 | allows for better thread usage | :white_check_mark: |

:warning: *means that library is provided to public headers, but not recommended to be used*

## Important note about licences:
Most of licences are permitted into engine code because they can be used in binary form without having to open source the engine itself.<br>
Libraries that require DLL linkage are not recommended. (sometimes they necessary like OpenAL or TBB)<br>
Libraries that require open source in both (static and dynamic) form of linkage are not permitted into engine.<br>

## Editor
An example of editor that uses such engine can be found [here](https://github.com/Rynnya/coffee-editor)