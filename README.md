# Vkapp

An **experimental** C++23 application bootstrapping library. It aims to bring more 'modern' C++ to Vulkan with desktop-related assumptions, some of the design decisions made are *weird*.

This repo is **experimental** and subject to change and break. Notably it uses C++20 modules (including the C++23 standard module), it is currently able to build on MSVC-latest.

### Dependencies
- [Vulkan](https://github.com/KhronosGroup/Vulkan-Hpp)
- [VulkanMemoryAllocator-Hpp](https://github.com/YaaZ/VulkanMemoryAllocator-Hpp)
- [SDL](https://github.com/libsdl-org/SDL)
- [glm](https://github.com/g-truc/glm)
- [Dear ImGui](https://github.com/ocornut/imgui)
- [Slang](https://github.com/shader-slang/slang)
- [spdlog](https://github.com/gabime/spdlog)

This has these following dependencies that can be fetched from vcpkg (to be made into a port).

### Usage
1. `git clone https://github.com/IyadHamid/Vkapp.git` in yout project
1. Edit `CMakeLists.txt`
    1. Assure `import std` is [enabled](https://cmake.org/cmake/help/latest/prop_tgt/CXX_MODULE_STD.html)
    1. Add the following
    ```cmake
    ...
    add_subdirectory(vkapp)
    target_link_libraries(<target> PRIVATE vkapp)
    ...
    ```
1. Add to `vcpkg.json`
    ```json
    "dependencies": [
        "glm",
        {
        "name": "imgui",
        "features": [
            "sdl3-binding",
            "vulkan-binding"
        ]
        },
        {
        "name": "sdl3",
        "features": [
            "vulkan"
        ]
        },
        "shader-slang",
        "spdlog",
        "vulkan",
        "vulkan-memory-allocator-hpp",
    ],
    "overrides": [
        {
        "name": "vulkan-memory-allocator",
        "version": "3.1.0"
        }
    ]
    ```

# TODOs
- Replace shader modules with pipelines
- Verify compatabilities with device
  - api version, present mode, swapchain format
- Replace `vkapp::meta` with C++26 reflection
- Convert into a vcpkg port
- Add examples