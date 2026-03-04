find_package(Vulkan REQUIRED)

if(${Vulkan_VERSION} VERSION_LESS "1.3.256")
    message(FATAL_ERROR "Minimum required Vulkan version for C++ modules is 1.3.256. Found ${Vulkan_VERSION}.")
endif()

add_library(VulkanHppModule)
add_library(Vulkan::HppModule ALIAS VulkanHppModule)

target_compile_definitions(VulkanHppModule
   PUBLIC
      VULKAN_HPP_NO_SMART_HANDLE
      VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
)

target_compile_features(VulkanHppModule PUBLIC cxx_std_23)
target_sources(VulkanHppModule
   PUBLIC
      FILE_SET CXX_MODULES 
      BASE_DIRS "${Vulkan_INCLUDE_DIR}"
      FILES 
        "${Vulkan_INCLUDE_DIR}/vulkan/vulkan_video.cppm"
        "${Vulkan_INCLUDE_DIR}/vulkan/vulkan.cppm" 
)
target_link_libraries(VulkanHppModule PUBLIC Vulkan::Vulkan Vulkan::Headers)