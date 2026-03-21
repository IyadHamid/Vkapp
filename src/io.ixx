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

using namespace std::literals;

namespace vkapp::io {
	export using Key = SDL_Scancode;
	export enum class MouseButton : SDL_MouseButtonFlags {
		Left = (SDL_BUTTON_LEFT)-1,
		Middle = (SDL_BUTTON_MIDDLE)-1,
		Right = (SDL_BUTTON_RIGHT)-1,
		X1 = (SDL_BUTTON_X1)-1,
		X2 = (SDL_BUTTON_X2)-1
	};
	export enum class MouseMove { X = 0, Y = 1 };
	export enum class MouseWheel { X = 0, Y = 1 };

	export using Input = std::variant<Key, MouseButton, MouseMove, MouseWheel>;
	export using Clicks = std::uint8_t;
	export struct MultiInput {
		Input input;
		Clicks count = 1;
	};
	export struct Combo {
		MultiInput input;
		std::vector<Input> modifiers = {};
		float scale = 1.f;
	};

	export struct InputEvent {
		Input input;
		float value;
		bool pressed;
		std::chrono::nanoseconds timestamp;
	};

	export class InputState {
		using EventQueue = std::vector<InputEvent>; // TODO[C++26]: inplace_vector<InputEvent, 2>

		static constexpr auto keys_count = SDL_SCANCODE_COUNT;
		static constexpr auto buttons_count = std::numeric_limits<SDL_MouseButtonFlags>::digits;

		glm::vec2 mouse_motion;
		glm::vec2 mouse_wheel;

		struct KeyboardState {
			using Keys = std::span<const bool, keys_count>;
			Keys keys;
		};
		static KeyboardState keyboardState() {
			return { KeyboardState::Keys(SDL_GetKeyboardState(nullptr), keys_count) };
		}
		struct MouseState {
			using Buttons = std::bitset<buttons_count>;
			Buttons buttons;
			glm::vec2 position;
		};
		static MouseState mouseState() {
			glm::vec2 position;
			auto buttons = SDL_GetMouseState(&position.x, &position.y);
			return { MouseState::Buttons(buttons), position };
		}

		static EventQueue buttonEvent(Input input, bool pressed, Uint64 timestamp) {
			return { InputEvent{ input, pressed ? 1.f : 0.f, pressed, std::chrono::nanoseconds(timestamp) } };
		}
		static EventQueue axisEvent(Input x, Input y, glm::vec2 value, Uint64 timestamp) {
			return { { x, value.x, true, std::chrono::nanoseconds(timestamp) }, { y, value.y, true, std::chrono::nanoseconds(timestamp) } };
		}
	public:

		EventQueue event(const SDL_KeyboardEvent& e) { return not e.repeat ? buttonEvent(e.scancode, e.down, e.timestamp) : EventQueue{}; }
		EventQueue event(const SDL_MouseButtonEvent& e) { return buttonEvent(MouseButton(e.button), e.down, e.timestamp); }
		EventQueue event(const SDL_MouseMotionEvent& e) { mouse_motion = glm::vec2(e.xrel, e.yrel); return axisEvent(MouseMove::X, MouseMove::Y, mouse_motion, e.timestamp); }
		EventQueue event(const SDL_MouseWheelEvent& e) { mouse_wheel = glm::vec2(e.x, e.y); return axisEvent(MouseWheel::X, MouseWheel::Y, mouse_wheel, e.timestamp); }
		EventQueue event(const SDL_Event& e) {
			switch (e.type) {
			case SDL_EVENT_KEY_DOWN:
			case SDL_EVENT_KEY_UP: return event(e.key);
			case SDL_EVENT_MOUSE_BUTTON_DOWN:
			case SDL_EVENT_MOUSE_BUTTON_UP: return event(e.button);
			case SDL_EVENT_MOUSE_MOTION: return event(e.motion);
			case SDL_EVENT_MOUSE_WHEEL: return event(e.wheel);
			default: break;
			}
			return {};
		}

		float value(Key key) const { return keyboardState().keys[key] ? 1.f : 0.f; }
		float value(MouseButton button) const { return mouseState().buttons[std::to_underlying(button)] ? 1.f : 0.f; }
		float value(MouseMove move) const { return mouse_motion[std::to_underlying(move)]; }
		float value(MouseWheel wheel) const { return mouse_wheel[std::to_underlying(wheel)]; }
		float value(Input input) const {
			return std::visit([&](auto i) { return value(i); }, input);
		}

		static std::string name(Key key) { return std::format("{}", SDL_GetScancodeName(key)); }
		static std::string name(MouseButton button) { return std::format("Button {}", std::to_underlying(button)); }
		static std::string name(MouseMove move) { return std::format("Move {}", move == MouseMove::X ? "Right"sv : "Up"sv); }
		static std::string name(MouseWheel wheel) { return std::format("Scroll {}", wheel == MouseWheel::X ? "Right"sv : "Up"sv); }
		static std::string name(Input input) {
			return std::visit([&](auto i) { return name(i); }, input);
		}

		glm::vec2 mousePosition() const { return mouseState().position; }
	};


}
#if 1
namespace vkapp {

	export template <typename AxisT>
	class InputManager {
	public:
		using axis_type = AxisT;
	protected:
		struct AxisControl {
			std::vector<io::Combo> combos;
			std::vector<std::size_t> active_combos = {};
			std::size_t tickgroup = 0;
		};
		struct ComboLookup {
			axis_type axis;
			std::size_t count;
			std::size_t index;
		};
		struct InputMultiClickInfo {
			std::chrono::nanoseconds last_timestamp = 0ns;
			io::Clicks count = 1;
			std::vector<ComboLookup> combos;
		};
	public:
		struct ControlEvent {
			axis_type axis;
			float value;
			std::chrono::nanoseconds timestamp;
		};
	protected:
		io::InputState state;

		std::unordered_map<axis_type, AxisControl> axis_controls = {};
		std::unordered_map<io::Input, InputMultiClickInfo> input_lookup = {};

		std::chrono::nanoseconds multi_click_duration = 300ms;
		float held_threshold = 0.1f;

		std::vector<std::deque<ControlEvent>> events = { {} };
		std::mutex event_mutex;

		bool inputHeld(io::Input input) const { return std::abs(state.value(input)) > held_threshold; }
		bool testModifiers(const io::Combo& combo) const { return std::ranges::all_of(combo.modifiers, [&](auto mod) { return inputHeld(mod); }); }

		void incrementClickCount(InputMultiClickInfo& info, std::chrono::nanoseconds timestamp) {
			bool is_multiclick = timestamp- info.last_timestamp <= multi_click_duration;
			info.count = is_multiclick ? info.count + 1 : 1;
			info.last_timestamp = timestamp;
		}
	public:

		InputManager() = default;

		float value(axis_type axis) { 
			AxisControl& control = axis_controls.at(axis);
			for (auto index : control.active_combos | std::views::reverse) {
				const io::Combo& combo = control.combos[index];
				if (testModifiers(combo))
					return state.value(combo.input.input) * combo.scale;
			}
			return 0.f;
		}
		float operator[](axis_type axis) { return value(axis); }
		bool held(axis_type axis, float tolerance = 0.f) { return std::abs(value(axis)) > tolerance; }

		template <typename... Axes> requires (sizeof...(Axes) > 1)
		glm::vec<sizeof...(Axes), float> operator[](Axes... axes) { return { value(axes)... }; }

		void tickgroup(std::size_t group, std::span<axis_type> axes) {
			for (auto axis : axes)
				axis_controls[axis].tickgroup = group;
			events.resize(group);
		}
		void tickgroup(std::size_t group, axis_type axis) { tickgroup(group, {{ axis }}); }

		std::size_t bind(axis_type axis, const io::Combo& combo);
		void unbind(axis_type axis, std::size_t combo_index);
		void unbindAll(axis_type axis);
		void refresh();

		void process(SDL_Event event);

		std::deque<ControlEvent> flush(std::size_t group = 0) {
			std::scoped_lock(event_mutex);
			return std::exchange(events[group], {});
		}


		glm::vec2 mousePosition() const { return state.mousePosition(); };
	};
}

// impl
using namespace vkapp;


template <typename AxisT>
std::size_t InputManager<AxisT>::bind(axis_type axis, const io::Combo& combo) {
	auto& combos = axis_controls.try_emplace(axis).first->second.combos;
	combos.push_back(combo);
	return combos.size() - 1;
}

template <typename AxisT>
void InputManager<AxisT>::unbind(axis_type axis, std::size_t combo_index) {
	auto& control = axis_controls.at(axis);
	auto& combos = control.combos;
	combos.erase(combos.begin() + combo_index);
	control.active_combos.clear();
	std::erase(control.active_combos, combo_index);
}

template <typename AxisT>
void InputManager<AxisT>::unbindAll(axis_type axis) {
	auto& control = axis_controls.at(axis);
	auto& combos = control.combos;
	combos.clear();
	control.active_combos.clear();
}

template <typename AxisT>
void InputManager<AxisT>::refresh() {
	input_lookup = {};
	for (auto&& [axis, control] : axis_controls) {
		for (auto&& [i, combo] : std::views::enumerate(control.combos)) {
			input_lookup[combo.input.input].combos.emplace_back(
				axis, combo.input.count, i
			);
		}
	}
}

template <typename AxisT>
void InputManager<AxisT>::process(SDL_Event event) {
	std::scoped_lock(event_mutex);
	for (auto&& e : state.event(event)) {
		auto it = input_lookup.find(e.input);
		if (it == input_lookup.end())
			continue;
		InputMultiClickInfo& info = it->second;

		if (e.pressed) {
			incrementClickCount(info, e.timestamp);

			for (auto&& axis_lookups : info.combos 
				| std::views::reverse 
				| std::views::chunk_by([&](const auto& a, const auto& b) {
					return a.axis == b.axis;
				})
			) {
				auto axis = axis_lookups[0].axis;
				auto& control = axis_controls[axis];
				std::optional<float> scale = std::nullopt;
				for (auto lookup : axis_lookups) {
					if (info.count % lookup.count != 0)
						continue;

					auto& combo = control.combos[lookup.index];

					control.active_combos.push_back(lookup.index);
					if (testModifiers(combo))
						scale = combo.scale;
				}
				if (scale)
					events[control.tickgroup].push_back({ axis, *scale * e.value, e.timestamp });
			}
		}
		else /* released */ {
			for (const auto& lookup : info.combos) {
				AxisControl& control = axis_controls[lookup.axis];
				std::erase(control.active_combos, lookup.index);
			}

		}
	}
}

#endif