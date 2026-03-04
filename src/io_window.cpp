module;

#include "log.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

module vkapp;

using namespace vkapp;

SDLGuard::SDLGuard() {
	check(SDL_Init(SDL_INIT_VIDEO), SDL_GetError());
}

void SDLGuard::destroy() const {
	SDL_Quit();
}

Window::Window(zstring_view name, vk::Extent2D resolution, std::uint64_t flags) :
	window{ SDL_CreateWindow(name.c_str(), resolution.width, resolution.height, flags | SDL_WINDOW_VULKAN) }
{

	const auto cursor_pos = [] {
		float x, y;
		SDL_GetGlobalMouseState(&x, &y);
		return SDL_Point{ static_cast<int>(x), static_cast<int>(y) };
		}();
	const auto display = SDL_GetDisplayForPoint(&cursor_pos);
	SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED_DISPLAY(display), SDL_WINDOWPOS_CENTERED_DISPLAY(display));
	check(window != nullptr, SDL_GetError());
	SPDLOG_INFO("created window");
}

Window::~Window() {
	SPDLOG_INFO("destroyed window");
}

std::span<const char* const> Window::getInstanceExtensions() const {
	std::uint32_t count;
	const auto raw = SDL_Vulkan_GetInstanceExtensions(&count);
	return std::span(raw, count);
}

vk::SurfaceKHR Window::createSurface(vk::Instance instance) {
	check(instance, "instance not valid");

	vk::SurfaceKHR::CType c_surface;
	check(SDL_Vulkan_CreateSurface(window, instance, nullptr, &c_surface), "failed to create surface");
	auto surface = vk::SurfaceKHR(c_surface);

	SPDLOG_INFO("created surface");
	return surface;
}

void Window::destroySurface(vk::Instance instance, vk::SurfaceKHR surface) {
	SDL_Vulkan_DestroySurface(instance, surface, nullptr);

	SPDLOG_INFO("destroyed surface");
}

vk::Extent2D Window::getWindowSize() const {
	int x, y;
	SDL_GetWindowSizeInPixels(window, &x, &y);
	[[assume(x >= 0 and y >= 0)]]
	return { static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y) };
}

std::generator<SDL_Event> Window::poll() const {
	for (SDL_Event event; SDL_PollEvent(&event);)
		co_yield event;
}