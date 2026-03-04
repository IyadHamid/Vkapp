module;

export module vkapp:glm_compat;

import std;
import glm;
import vulkan;

import :utils;

export namespace vkapp {
    constexpr auto glm_vec(vk::Extent2D extent) { return glm::uvec2(extent.width, extent.height); }
    constexpr auto glm_vec(vk::Extent3D extent) { return glm::uvec3(extent.width, extent.height, extent.depth); }


    template <typename T>
    struct glm_format;
#define GLM_FMTS(TYPE, SZ, SUFFIX) \
    template <> struct glm_format<            TYPE  > : std::integral_constant<vk::Format, vk::Format:: eR ## SZ ## SUFFIX > {}; \
    template <> struct glm_format<glm::vec<1, TYPE >> : std::integral_constant<vk::Format, vk::Format:: eR ## SZ ## SUFFIX > {}; \
    template <> struct glm_format<glm::vec<2, TYPE >> : std::integral_constant<vk::Format, vk::Format:: eR ## SZ ## G ## SZ ## SUFFIX > {}; \
    template <> struct glm_format<glm::vec<3, TYPE >> : std::integral_constant<vk::Format, vk::Format:: eR ## SZ ## G ## SZ ## B ## SZ ## SUFFIX > {}; \
    template <> struct glm_format<glm::vec<4, TYPE >> : std::integral_constant<vk::Format, vk::Format:: eR ## SZ ## G ## SZ ## B ## SZ ## A ## SZ ## SUFFIX > {};


    GLM_FMTS(glm::i8 ,  8, Sint);
    GLM_FMTS(glm::i16, 16, Sint);
    GLM_FMTS(glm::i32, 32, Sint);
    GLM_FMTS(glm::i64, 64, Sint);

    GLM_FMTS(glm::u8 ,  8, Uint);
    GLM_FMTS(glm::u16, 16, Uint);
    GLM_FMTS(glm::u32, 32, Uint);
    GLM_FMTS(glm::u64, 64, Uint);

    GLM_FMTS(glm::f32, 32, Sfloat);
    GLM_FMTS(glm::f64, 64, Sfloat);
#undef GLM_FMTS

    template <typename T>
    constexpr vk::Format glm_format_v = glm_format<T>::value;

    template <meta::decomposable T>
    constexpr auto makeVertexInputAttribute(std::uint32_t binding = 0) {
        return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            return std::array{ vk::VertexInputAttributeDescription2EXT(Is, binding, glm_format<meta::member_t<Is, T>>::value, meta::getOffsets<T>()[Is])... };
        }(std::make_index_sequence<meta::memberCount<T>()>{});
    }
}

export template <int N, typename T, glm::qualifier Q>
struct std::formatter<glm::vec<N, T, Q>> {
    using vec = glm::vec<N, T, Q>;
    using span = std::span<const T, N>;

    std::range_formatter<T> range_formatter;

    constexpr formatter() { range_formatter.set_brackets("(", ")"); }

    constexpr auto parse(auto& ctx) { return range_formatter.parse(ctx); }

    auto format(const vec& v, auto& ctx) const { return range_formatter.format(span(glm::gtc::value_ptr(v), N), ctx); }
};