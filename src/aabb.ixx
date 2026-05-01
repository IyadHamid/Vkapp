module;

export module vkapp:aabb;

import std;
import glm;

namespace vkapp {
	export template <typename T = float>
		requires std::is_arithmetic_v<T>
	struct AABB {
		using element_type = T;
		using vec = glm::vec<2, element_type>;
		vec min = {};
		vec max = {};

		static constexpr element_type rounding_epsilon = element_type(0.0001);

	private:
		static constexpr vec floor(vec x) { return glm::floor(x + rounding_epsilon); }
		static constexpr vec ceil(vec x) { return glm::ceil(x - rounding_epsilon); }

	public:

		constexpr bool valid() const { return all(lessThan(min, max)); }
		explicit constexpr operator bool() const { return valid(); }

		constexpr vec extent() const { return max - min; }

		template <typename U>
		explicit constexpr operator AABB<U>() const {
			if constexpr (std::floating_point<element_type> and std::integral<U>)
				return { AABB<U>::vec(floor(min)), AABB<U>::vec(ceil(max)) };
			else if constexpr (std::floating_point<element_type> and std::floating_point<U> and sizeof(U) < sizeof(element_type))
				return { AABB<U>::vec(min - rounding_epsilon), AABB<U>::vec(max + rounding_epsilon) };
			else
				return { AABB<U>::vec(min), AABB<U>::vec(max) };
		}
		template <std::integral U, U Chunk>
		constexpr AABB<U> align() const {
			constexpr auto inv_chunk = element_type(1) / static_cast<element_type>(Chunk);
			if constexpr (std::floating_point<element_type>)
				return { AABB<U>::vec(floor(min * inv_chunk)) * Chunk, AABB<U>::vec(ceil(max * inv_chunk)) * Chunk };
			else
				return { AABB<U>::vec(min * inv_chunk) * Chunk, AABB<U>::vec(max * inv_chunk) * Chunk };
		}

		constexpr bool contains(const vec& point) const {
			return all(lessThanEqual(min, point) and lessThan(point, max));
		}

		friend constexpr bool operator==(const AABB& a, const AABB& b) { return a.min == b.min and a.max == b.max; }

		constexpr AABB offset(vec x) const { return { min + x, max + x }; };
		constexpr AABB scale(vec x) const { return { min * x, max * x }; }
		constexpr AABB scale(element_type x) const { return scale(vec(x, x)); }
		constexpr AABB expand(vec x) const { return { min - x, max + x }; }
		constexpr AABB expand(element_type x) const { return expand(vec(x, x)); }

		friend constexpr AABB mix(const AABB& a, const AABB& b, float t) { return { mix(a.min, b.min, t), mix(a.max, b.max, t) }; }

		friend constexpr bool intersects(const AABB& a, const AABB& b) {
			return all(lessThanEqual(a.min, b.max) and lessThanEqual(b.min, a.max));
		}

		//       B-------B
		//       |       |
		//    A--+----A  |
		//    |  |####|  |
		//    |  B----+--B
		//    |       |   
		//    A-------A   
		friend constexpr AABB intersection(const AABB& a, const AABB& b) {
			return { glm::max(a.min, b.min), glm::min(a.max, b.max) };
		}

		//    ...B-------B
		//    .##|#######|
		//    A--+----A##|
		//    |##|####|##|
		//    |##B----+--B
		//    |#######|##.
		//    A-------A...
		friend constexpr AABB add(const AABB& a, const AABB& b) {
			return { glm::min(a.min, b.min), glm::max(a.max, b.max) };
		}

		//    A----------A
		//    |3#########|
		//    |..B----B..|
		//    |1#|    |2#|
		//    |..B----B..|
		//    |0#########|
		//    A----------A
		friend constexpr std::array<AABB, 4> sub(const AABB& a, const AABB& b) {
			return {
				AABB{ vec(a.min.x, a.min.y), vec(a.max.x, b.min.y) },
				AABB{ vec(a.min.x, b.min.y), vec(b.min.x, b.max.y) },
				AABB{ vec(b.max.x, b.min.y), vec(a.max.x, b.max.y) },
				AABB{ vec(a.min.x, b.max.y), vec(a.max.x, a.max.y) },
			};
		}


		struct range_sentinel {};
		struct range_iterator {
			AABB bounds;
			vec current;
			vec step = { 1, 1 };

			using value_type = vec;
			using difference_type = std::ptrdiff_t;
			using iterator_category = std::forward_iterator_tag;

			value_type operator*() const { return current; }

			range_iterator& operator++() {
				current.x += step.x;
				if (current.x >= bounds.max.x) {
					current.x = bounds.min.x;
					current.y += step.y;
				}
				return *this;
			}
			range_iterator operator++(int) {
				auto temp = *this;
				++(*this);
				return temp;
			}

			bool operator==(range_sentinel) const { return current.y >= bounds.max.y; }

		};
		struct range_wrapper {
			AABB bounds;
			vec step = { 1, 1 };
			auto begin() const { return range_iterator{ bounds, bounds.min, step }; }
			auto end() const { return range_sentinel{}; }
		};

		auto range(vec step = { 1, 1 }) const { return range_wrapper{ *this, step }; }
	};
}