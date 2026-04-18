# Vkapp

An **experimental** C\++23 application bootstrapping library. It aims to bring more 'modern' C\++ to Vulkan with desktop-related assumptions, some of the design decisions made are *weird*.

This repo is **experimental** and subject to change and break. It currently builds with MSVC latest.

## Features/Oddities
- Module-only requiring the `std` module and other library modules
    - Uses the `vulkan` experimental module (potentially breaking)
- Uses reflection
    - Used to generate RAII for vulkan-device managed types (does not use `vk::raii`)
    - Will also be used for auto debug naming

## Dependencies
- [Vulkan](https://github.com/KhronosGroup/Vulkan-Hpp) - graphics
- [VulkanMemoryAllocator-Hpp](https://github.com/YaaZ/VulkanMemoryAllocator-Hpp) - memory allocation
- [SDL](https://github.com/libsdl-org/SDL) - windowing + io library
- [glm](https://github.com/g-truc/glm) - basic linalg
- [Dear ImGui](https://github.com/ocornut/imgui) - debug screen rendering
- [Slang](https://github.com/shader-slang/slang) - shader language
- [spdlog](https://github.com/gabime/spdlog) - logging (including for vulkan messages)

This has these following dependencies that can be fetched from vcpkg (to be made into a port).

## Usage
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
    ]
    ```

## TODOs
- Verify compatabilities with device
  - api version, present mode, swapchain format
- Replace `vkapp::meta` with C++26 reflection
  - add auto debug naming (rm `app.meowAndNameAs`)
- Convert into a vcpkg port
- Add examples