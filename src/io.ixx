module;

#include "log.h"
#include <SDL3/SDL.h>

export module vkapp:io;

import :utils;

import vulkan;
import glm;

namespace vkapp {
	struct SDLGuard : GlobalState<SDLGuard> {
		SDLGuard();
		void destroy() const;
	};

	class Window : SDLGuard {
	public:
		SDL_Window* window;
		

		Window(zstring_view name, vk::Extent2D resolution, std::uint64_t flags = SDL_WINDOW_RESIZABLE);
		~Window();

		[[nodiscard]] std::span<const char* const> getInstanceExtensions() const;

		vk::SurfaceKHR createSurface(vk::Instance instance);
		void destroySurface(vk::Instance instance, vk::SurfaceKHR surface);

		[[nodiscard]] vk::Extent2D getWindowSize() const;

		std::generator<SDL_Event> poll() const;
	};
}

export namespace vkapp {
	template <typename AxisT>
	class InputManager {
		// TODO: add Events + double-click
	public:
		using axis_type = AxisT;
		enum class MouseMove { X = 0, Y = 1 };
		enum class MouseButton : Uint8 { 
			Left = (SDL_BUTTON_LEFT) - 1, 
			Middle = (SDL_BUTTON_MIDDLE) - 1,
			Right = (SDL_BUTTON_RIGHT) - 1,
			X1 = (SDL_BUTTON_X1) - 1,
			X2 = (SDL_BUTTON_X2) - 1
		};

		using AxisControl = std::variant<SDL_Scancode, MouseMove, MouseButton>;

	protected:
		struct ScaledAxisControl {
			AxisControl control;
			float scale = 1.f;
		};

		struct MouseState {
			static constexpr auto number_of_buttons = std::bit_width(std::numeric_limits<SDL_MouseButtonFlags>::max());
			std::bitset<number_of_buttons> buttons = 0;
			glm::vec2 position = glm::vec2(0.f);
			glm::vec2 delta = glm::vec2(0.f);
		};


		// TODO: add gamepad support, scroll wheel

		using axis_value = std::pair<axis_type, float>;

		std::unordered_map<axis_type, float> axis_map = {};
		std::unordered_multimap<axis_type, ScaledAxisControl> axis_controls = {};

		glm::vec2 mouse_delta = {};


		std::span<const bool> getKeyboardState() const;
		MouseState getMouseState() const;

		float getControlValue(const AxisControl& control) const;

	public:

		InputManager() = default;

		float operator[](axis_type axis) const;
		bool held(axis_type axis, float tolerance = 0.f) const;

		template <typename... Axes> requires (sizeof...(Axes) > 1)
		glm::vec<sizeof...(Axes), float> operator[](Axes... axes) const { return { (*this)[axes]... }; }

		void bind(axis_type axis, const AxisControl& control, float scale = 1.f);
		void unbind(axis_type axis, const AxisControl& control);
		void unbindAll(axis_type axis);

		void processReset();
		bool process(SDL_Event event);
		void processFinish();

		glm::vec2 getMousePosition();
	};
}

// impl
using namespace vkapp;

template <typename AxisT>
std::span<const bool> InputManager<AxisT>::getKeyboardState() const {
	int count;
	auto state = SDL_GetKeyboardState(&count);
	return std::span(state, count);
}

template <typename AxisT>
InputManager<AxisT>::MouseState InputManager<AxisT>::getMouseState() const {
	float x, y;
	auto buttons = SDL_GetMouseState(&x, &y);
	return { { buttons }, { x, y }, mouse_delta };
}

template <typename AxisT>
float InputManager<AxisT>::getControlValue(const AxisControl& control) const {
	return std::visit([&](auto&& control) -> float {
		using type = std::decay_t<decltype(control)>;
		if constexpr (std::same_as<type, SDL_Scancode>)
			return getKeyboardState()[control] ? 1.f : 0.f;
		if constexpr (std::same_as<type, MouseMove>)
			return getMouseState().delta[std::to_underlying(control)];
		if constexpr (std::same_as<type, MouseButton>)
			return getMouseState().buttons[std::to_underlying(control)] ? 1.f : 0.f;
		std::unreachable();
	}, control);
}



template <typename AxisT>
float InputManager<AxisT>::operator[](axis_type axis) const {
	return axis_map.contains(axis) ? axis_map.at(axis) : 0.f;
}

template <typename AxisT>
bool InputManager<AxisT>::held(axis_type axis, float tolerance) const {
	return std::abs((*this)[axis]) > tolerance;
}

template <typename AxisT>
void InputManager<AxisT>::bind(axis_type axis, const AxisControl& control, float scale) {
	axis_controls.emplace(axis, ScaledAxisControl{ control, scale });
	axis_map.try_emplace(axis, 0.f);
}

template <typename AxisT>
void InputManager<AxisT>::unbind(axis_type axis, const AxisControl& control) {
	auto range = axis_controls.equal_range(axis);
	for (auto it = range.first; it != range.second; ++it)
		if (it->second.control == control) {
			axis_controls.erase(it);
			return;
		}
}

template <typename AxisT>
void InputManager<AxisT>::unbindAll(axis_type axis) {
	auto range = axis_controls.equal_range(axis);
	axis_controls.erase(range.first, range.second);
}

template <typename AxisT>
void InputManager<AxisT>::processReset() {
	mouse_delta = glm::vec2(0.f);
}

template <typename AxisT>
bool InputManager<AxisT>::process(SDL_Event event) {
	switch (event.type) {
	case SDL_EVENT_MOUSE_MOTION:
		mouse_delta += glm::vec2{ event.motion.xrel, -event.motion.yrel }; // top right is (+,+)
		break;
	default: return false;
	}
	return true;
}

template <typename AxisT>
void InputManager<AxisT>::processFinish() {
	for (auto&& [axis, value] : axis_map) {
		value = 0.f;
		auto range = axis_controls.equal_range(axis);
		for (auto it = range.first; it != range.second; ++it) {
			const auto& [control, scale] = it->second;
			auto control_value = getControlValue(control);
			value += control_value * scale;
		}
	}
}

template <typename AxisT>
glm::vec2 InputManager<AxisT>::getMousePosition() {
	float x, y;
	SDL_GetMouseState(&x, &y);
	return { x, y };
}