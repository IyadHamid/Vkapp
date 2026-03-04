module;

#include <cassert>

export module vkapp:utils;

import std;

export namespace vkapp {
	template <std::unsigned_integral I = unsigned>
	struct single_bit_t {
		using type = I;
		
		type bit_shift;

		[[nodiscard]] constexpr type shift() const noexcept { return bit_shift; }
		[[nodiscard]] constexpr type value() const noexcept { return 1 << bit_shift; }
		[[nodiscard]] constexpr operator type() const noexcept { return value(); }
		[[nodiscard]] constexpr type mask() const noexcept { return value() - 1; }
	};

	using ubit_t = single_bit_t<unsigned>;

	namespace literals {

		constexpr ubit_t operator""_ub(unsigned long long int b) {
			assert(std::has_single_bit(b));
			return ubit_t{ static_cast<unsigned>(std::bit_width(b) - 1) };
		}

	}
}

export namespace vkapp {
	template <class Char>
	struct basic_zstring_view : std::basic_string_view<Char> {
		using string_view = std::basic_string_view<Char>;
		using string_view::string_view;
		basic_zstring_view(const string_view&) = delete;
		constexpr basic_zstring_view(const std::string& s) : string_view(s) {}

		using string_view::data;
		using string_view::size;

		constexpr operator string_view() const { return { data(), size() }; }
		constexpr operator const Char*() const { return data(); }
		[[nodiscard]] constexpr auto c_str() const { return data(); }
	};
	using zstring_view = basic_zstring_view<char>;
	namespace literals {
		consteval zstring_view operator""_zv(const char* s, std::size_t) { return { s }; }
	}

	std::string indexName(std::string_view name, std::size_t index) {
		return std::format("{}[{}]", name, index);
	}
	auto indexedNames(std::string_view name) {
		return std::views::iota(0uz) | std::views::transform([name](std::size_t i) { return indexName(name, i); });
	}
	auto indexedNames(std::string_view name, std::size_t count) {
		return std::views::iota(0uz, count) | std::views::transform([name](std::size_t i) { return indexName(name, i); });
	}
}

export namespace vkapp {
	auto currentRelevantStacktrace(std::stacktrace::difference_type drop = 0) {
		return std::stacktrace::current()
		| std::views::drop(drop + 1)
		| std::views::filter([](auto&& s) { return not (s.source_file().empty() or s.source_line() == 0); });
	}

}

export template <class Char>
struct std::formatter<vkapp::basic_zstring_view<Char>> : std::formatter<std::basic_string_view<Char>> {};

export namespace vkapp {
	// TODO[C++26: reflection]: rewrite
#ifndef __cpp_lib_reflection
	namespace meta {
		// NOLINTBEGIN
		struct AnyType { template <typename T> operator T(); };
		template <typename T>
		struct BaseType { template <typename U> requires (std::is_base_of_v<U, T>) operator U(); };
		// NOLINTEND

		template <typename T, typename... Args>
		consteval auto memberCount() requires std::is_aggregate_v<T> {
			if constexpr (requires {T{ Args{}..., AnyType{} }; } == false)
				return sizeof...(Args);
			else if constexpr (requires {T{ Args{}..., BaseType<T>{} }; } == true)
				return memberCount<T, Args..., BaseType<T>>() - 1;
			else
				return memberCount<T, Args..., AnyType>();
		}

		template <typename T, std::size_t N>
		concept aggregate_n = std::is_aggregate_v<std::remove_cvref_t<T>> and memberCount<std::remove_cvref_t<T>>() == N;

#define DECOMPOSE_F(N, ...) constexpr auto decompose(aggregate_n< N > auto&& object) { auto&& [ __VA_ARGS__ ] = object; return std::tie( __VA_ARGS__ ); }
		DECOMPOSE_F(20, o0, o1, o2, o3, o4, o5, o6, o7, o8, o9, o10, o11, o12, o13, o14, o15, o16, o17, o18, o19);
		DECOMPOSE_F(19, o0, o1, o2, o3, o4, o5, o6, o7, o8, o9, o10, o11, o12, o13, o14, o15, o16, o17, o18);
		DECOMPOSE_F(18, o0, o1, o2, o3, o4, o5, o6, o7, o8, o9, o10, o11, o12, o13, o14, o15, o16, o17);
		DECOMPOSE_F(17, o0, o1, o2, o3, o4, o5, o6, o7, o8, o9, o10, o11, o12, o13, o14, o15, o16);
		DECOMPOSE_F(16, o0, o1, o2, o3, o4, o5, o6, o7, o8, o9, o10, o11, o12, o13, o14, o15);
		DECOMPOSE_F(15, o0, o1, o2, o3, o4, o5, o6, o7, o8, o9, o10, o11, o12, o13, o14);
		DECOMPOSE_F(14, o0, o1, o2, o3, o4, o5, o6, o7, o8, o9, o10, o11, o12, o13);
		DECOMPOSE_F(13, o0, o1, o2, o3, o4, o5, o6, o7, o8, o9, o10, o11, o12);
		DECOMPOSE_F(12, o0, o1, o2, o3, o4, o5, o6, o7, o8, o9, o10, o11);
		DECOMPOSE_F(11, o0, o1, o2, o3, o4, o5, o6, o7, o8, o9, o10);
		DECOMPOSE_F(10, o0, o1, o2, o3, o4, o5, o6, o7, o8, o9);
		DECOMPOSE_F( 9, o0, o1, o2, o3, o4, o5, o6, o7, o8);
		DECOMPOSE_F( 8, o0, o1, o2, o3, o4, o5, o6, o7);
		DECOMPOSE_F( 7, o0, o1, o2, o3, o4, o5, o6);
		DECOMPOSE_F( 6, o0, o1, o2, o3, o4, o5);
		DECOMPOSE_F( 5, o0, o1, o2, o3, o4);
		DECOMPOSE_F( 4, o0, o1, o2, o3);
		DECOMPOSE_F( 3, o0, o1, o2);
		DECOMPOSE_F( 2, o0, o1);
		DECOMPOSE_F( 1, o0);
#undef DECOMPOSE_F

		template <typename T>
		concept decomposable = requires(T & object) { decompose(object); };

		template <typename T>
		using decomposed_t = decltype(decompose(std::declval<T>()));

		template <std::size_t I, typename T>
		using member_t = std::remove_cvref_t<std::tuple_element_t<I, decomposed_t<T>>>;

		template <typename T>
		consteval auto getOffsets() {
			return []<std::size_t... Is>(std::index_sequence<Is...>) consteval {
				auto align = [](std::size_t alignment, std::size_t& vptr) {
					// from MSVC STL <xmemory> std::align
					std::size_t off = vptr & (alignment - 1);
					if (off != 0)
						off = alignment - off;
					return vptr += off;
				};
				std::array<std::size_t, sizeof...(Is)> offsets;
				const std::array sizes = { sizeof(member_t<Is, T>)... };
				const std::array alignments = { alignof(member_t<Is, T>)... };
				for (std::size_t i = 0, vptr = 0; i < sizeof...(Is); ++i) {
					offsets[i] = align(alignments[i], vptr);
					vptr += sizes[i];
				}
				return offsets;
			}(std::make_index_sequence<memberCount<T>()>{});
		}
	}

	template <typename, std::size_t>
	struct SkipNone : std::bool_constant<false> {};

	template <template <typename, std::size_t> class Skip = SkipNone, bool Reverse = false>
	void decomposeRecursive(auto&& f, auto&& object) {
		using namespace meta;
		using O = decltype(object);
		static auto nop = [] {};

		if constexpr (requires { f(object); }) {
			f(std::forward<O>(object));
		}
		else if constexpr (std::ranges::range<O>) {
			if constexpr (Reverse)
				for (auto&& o : object | std::views::reverse)
					decomposeRecursive<Skip>(f, o);
			else
				for (auto&& o : object)
					decomposeRecursive<Skip>(f, o);
		}
		else if constexpr (decomposable<O>) {
			auto decomposed = decompose(object);
			[&]<std::size_t... Is>(std::index_sequence<Is...>) {
				if constexpr (Reverse)
					(..., (Skip<std::remove_cvref_t<O>, Is>::value ? nop() : decomposeRecursive<Skip>(f, std::get<Is>(decomposed))));
				else
					((Skip<std::remove_cvref_t<O>, Is>::value ? nop() : decomposeRecursive<Skip>(f, std::get<Is>(decomposed))), ...);
			}(std::make_index_sequence<std::tuple_size_v<decltype(decomposed)>>{});
		}
	}

	auto decomposeAllGet(meta::decomposable auto&& struct_of_arrays, std::size_t i) {
		return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
			auto decomposed = meta::decompose(std::forward<decltype(struct_of_arrays)>(struct_of_arrays));
			return std::tie(std::get<Is>(decomposed)[i]...);
		}(std::make_index_sequence<meta::memberCount<std::remove_cvref_t<decltype(struct_of_arrays)>>()>{});
	}

	template <typename T, std::size_t I>
	struct destroy_skip : std::bool_constant<false> {};

	template <typename T>
	void destroyRecursive(auto& owner, T& object) {
		decomposeRecursive<destroy_skip, true>([&](auto& o) requires requires { owner.destroy(o); } {
			owner.destroy(o);
			if constexpr(requires { o = nullptr; }) // TODO[delete]: set to nullptr at device.destroy()
				o = nullptr;
		}, object);
	}
#else
	// TODO impl with reflection
#endif
}
export namespace vkapp {
	using float_seconds = std::chrono::duration<float>;
	using double_seconds = std::chrono::duration<double>;

	template <class Clock = std::chrono::high_resolution_clock, class Dur = std::chrono::microseconds, class Rate = void>
	class Timer {
	public:
		using time_point = std::chrono::time_point<Clock>;
		using duration = Dur;
		using rate = Rate;

		static constexpr bool is_fixed = not std::is_void_v<rate>;
		static constexpr duration one_tick = [] {
			if constexpr (is_fixed)
				return std::chrono::duration_cast<duration>(rate(1));
			else
				return duration{ 0 };
		}();

		time_point start, last;
		Timer() : start{ Clock::now() }, last{ start } {}

		void reset() { start = Clock::now(); }

		duration dt() requires not is_fixed {
			auto now = Clock::now();
			return std::chrono::duration_cast<duration>(now - std::exchange(last, now));
		}

		duration dt() requires is_fixed {
			std::this_thread::sleep_until(last + rate(1));
			last = Clock::now();
			return one_tick;
		}

		duration time() { return std::chrono::duration_cast<duration>(Clock::now() - start); }

		duration timeSinceDt() { return std::chrono::duration_cast<duration>(Clock::now() - last); }
		auto ratioSinceDt() requires is_fixed { return timeSinceDt() / one_tick; }
	};
}


namespace vkapp {
	template <class T>
	struct GlobalState {
		static std::atomic_uint count;
		GlobalState() { 
			++count; 
		}
		~GlobalState() {
			if (--count == 0)
				static_cast<T*>(this)->destroy();
		}
		void destroy() const {};
	};
	template <class T> 
	std::atomic_uint GlobalState<T>::count = 0u;
}
