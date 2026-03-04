find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(unofficial-vulkan-memory-allocator-hpp CONFIG REQUIRED)

get_target_property(VMAHPP_INCLUDE_DIR unofficial::VulkanMemoryAllocator-Hpp::VulkanMemoryAllocator-Hpp INTERFACE_INCLUDE_DIRECTORIES)

add_library(VulkanMemoryAllocator-Hpp-Module)

target_compile_features(VulkanMemoryAllocator-Hpp-Module PUBLIC cxx_std_23)
set_target_properties(VulkanMemoryAllocator-Hpp-Module PROPERTIES CXX_MODULE_STD ON)
target_sources(VulkanMemoryAllocator-Hpp-Module 
    PUBLIC 
        FILE_SET CXX_MODULES 
        BASE_DIRS "${VMAHPP_INCLUDE_DIR}/vulkan-memory-allocator-hpp"
        FILES "${VMAHPP_INCLUDE_DIR}/vulkan-memory-allocator-hpp/vk_mem_alloc.cppm"
)
target_link_libraries(VulkanMemoryAllocator-Hpp-Module
    PRIVATE Vulkan::HppModule GPUOpen::VulkanMemoryAllocator unofficial::VulkanMemoryAllocator-Hpp::VulkanMemoryAllocator-Hpp
)
target_include_directories(VulkanMemoryAllocator-Hpp-Module PRIVATE "${VMAHPP_INCLUDE_DIR}/vulkan-memory-allocator-hpp")