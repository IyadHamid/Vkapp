module;

#include "log.h"

export module vkapp:device_owner;

import std;
import vulkan;
import vk_mem_alloc_hpp;

import :utils;

namespace vkapp {
	void setDebugName(vk::Device device, auto object, zstring_view name) requires vk::isVulkanHandleType<std::remove_cvref_t<decltype(object)>>::value {
		device.setDebugUtilsObjectNameEXT(vk::DebugUtilsObjectNameInfoEXT(
			object.objectType,
			reinterpret_cast<std::uint64_t>(static_cast<typename decltype(object)::CType>(object)),
			name
		));
	}
	void setDebugNames(vk::Device device, std::ranges::input_range auto&& objects, std::string_view name) {
		auto zip = std::views::zip(objects, indexedNames(name));
		for (auto [object, name] : zip)
			setDebugName(device, object, name);
	}
}

namespace vkapp {
	export template <typename T>
	struct Destroyer;

	export struct DeviceOwner {
		vk::Device device;
		vma::Allocator allocator;


		auto&& nameAs(zstring_view name, auto&& object) const requires not std::ranges::input_range<decltype(object)> {
			setDebugName(device, std::forward<decltype(object)>(object), name);
			return static_cast<decltype(object)>(object);
		}
		auto&& nameAs(std::string_view name, std::ranges::input_range auto&& objects) const {
			setDebugNames(device, std::forward<decltype(objects)>(objects), name);
			return static_cast<decltype(objects)>(objects);
		}
		void moveAndNameAs(std::string_view name, std::ranges::range auto&& out, std::ranges::input_range auto&& in) { std::ranges::move(in, out.begin()); nameAs(name, std::forward<decltype(out)>(out)); }
		void generateAndNameAs(std::string_view name, std::ranges::range auto&& out, std::invocable auto&& f) { std::ranges::generate(out, f); nameAs(name, std::forward<decltype(out)>(out)); }
		
		template <typename T>
		void destroy(T& object) requires requires { Destroyer<T>{}; } { Destroyer<T>{}(*this, object); }

		void destroyAll(auto&& object) { destroyRecursive(*this, std::forward<decltype(object)>(object)); }

	public:
		template <typename T>
		struct Deleter {
			DeviceOwner owner;
			void operator()(T* ptr) noexcept {
				if (not ptr)
					return;
				owner.device.waitIdle();
				if constexpr (requires { owner.destroy(*ptr); }) {
					owner.destroy(*ptr);
				}
				else {
					owner.destroyAll(*ptr);
				}
				delete ptr;
			}
		};
		template <typename T>
		[[nodiscard]] auto deleter() { return Deleter<T>{ *this }; }

	public:
		template <typename T>
		using Unique = std::unique_ptr<T, Deleter<T>>;


		template <typename T>
		[[nodiscard]] auto makeUnique() { return Unique<T>(nullptr, deleter<T>()); }
		template <typename T>
		[[nodiscard]] auto makeUnique(std::nullptr_t) { return makeUnique<T>(); }
		[[nodiscard]] auto makeUnique(auto&& arg) { using T = std::remove_cvref_t<decltype(arg)>; return Unique<T>(new T(std::forward<decltype(arg)>(arg)), deleter<T>()); }

		[[nodiscard]] auto makeUniqueWithName(zstring_view name, auto&& arg) { return makeUnique(nameAs(name, std::forward<decltype(arg)>(arg))); }

	};

	export template <typename T>
	using Unique = DeviceOwner::Unique<T>;

	export template <typename T> requires requires { vk::Device{}.destroy(T{}); }
	struct Destroyer<T> {
		static void operator()(DeviceOwner owner, T& object) { owner.device.destroy(object); }
	};

}
