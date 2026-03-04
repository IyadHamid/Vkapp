cmake_minimum_required(VERSION 3.30)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

project("vkapp" LANGUAGES CXX)


find_package(imgui CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_library(SLANG slang REQUIRED)
find_package(spdlog CONFIG REQUIRED)
include(GLMModule)
include(VulkanHppModule)
include(VulkanMemoryAllocatorHppModule)

add_compile_definitions(SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)

add_library(vkapp "src/log.h")
target_compile_features(vkapp PUBLIC cxx_std_23)
target_sources(vkapp
	PRIVATE
		"src/functional.cpp"
		"src/shader.cpp"
		"src/app.cpp"
		"src/io_window.cpp"
		
	PUBLIC
		FILE_SET cxx_modules TYPE CXX_MODULES 
		BASE_DIRS
			"src"
		FILES
			"src/vkapp.ixx"
			"src/utils.ixx"
			"src/io.ixx"
			"src/shader.ixx"
			"src/functional.ixx"
			"src/glmcompat.ixx"
			"src/deviceowner.ixx"
			"src/resource.ixx"
			"src/app.ixx"
)
target_include_directories(vkapp PUBLIC "src")

target_link_libraries(vkapp PUBLIC 
	imgui::imgui
	SDL3::SDL3
	${SLANG}
	spdlog::spdlog_header_only
	glm-module
	Vulkan::HppModule Vulkan::Headers
	VulkanMemoryAllocator-Hpp-Module
)