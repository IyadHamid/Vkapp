module;

#include "log.h"
#include <vulkan/vulkan_format_traits.hpp>

export module vkapp:functional;

import std;
import vulkan;
import vk_mem_alloc_hpp;

import :utils;

namespace vk::detail {
	// #include <vulkan/vulkan_hpp_macros.hpp>
	export extern DispatchLoaderDynamic defaultDispatchLoaderDynamic;
}

export namespace vkapp {


	struct Queues {
		using QueuesEnum = std::uint8_t;
		enum : QueuesEnum { Graphics, Transfer, Presents, Count };

		std::array<std::uint32_t, Count> family_indices = {};
		std::array<std::uint32_t, Count> queue_indices = {};
		std::array<vk::Queue, Count> queues = {};
	};

	struct Swapchain {
		vk::SwapchainKHR swapchain;
		vk::Format format;
		std::vector<vk::Image> images;
		std::vector<vk::ImageView> image_views;
	};
	template <>
	struct destroy_skip<Swapchain, 2> : std::bool_constant<true> {};

	struct LayerDetails {
		using Properties = vk::LayerProperties;
		static constexpr auto name = &vk::LayerProperties::layerName;
		static auto all() { 
			return vk::enumerateInstanceLayerProperties<std::allocator<vk::LayerProperties>, vk::detail::DispatchLoaderDynamic, 0, true>(); 
		}
	};
	struct ExtensionDetails {
		using Properties = vk::ExtensionProperties;
		static constexpr auto name = &vk::ExtensionProperties::extensionName;
		static auto all() { 
			return vk::enumerateInstanceExtensionProperties<std::allocator<vk::ExtensionProperties>, vk::detail::DispatchLoaderDynamic, 0, true>(); 
		}
	};
	template <class Details>
	std::vector<const char*> gather(std::span<const char* const> requests = {}) {
		std::vector<const char*> enabled;
		enabled.reserve(requests.size());
		auto has = [all = Details::all()](zstring_view req) { return std::ranges::contains(all, req, Details::name); };
		for (const auto& req : requests) {
			check(has(req), "requested does not exist");
			enabled.push_back(req);
		}
		return enabled;
	}

	using QueueFamilyInfo = std::pair<std::uint32_t, const vk::QueueFamilyProperties&>;

	std::optional<std::uint32_t> findQueueFamilyIndex(vk::PhysicalDevice physical_device, std::invocable<const QueueFamilyInfo&> auto&& condition) {
		auto properties = physical_device.getQueueFamilyProperties();
		auto view = std::views::zip(std::views::iota(0u), properties);
		auto iter = std::ranges::find_if(view, condition);
		if (iter == view.end())
			return std::nullopt;
		return std::get<0>(*iter);
	}


	template <typename T>
	T findSupported(vk::Flags<T> supported, std::same_as<T> auto... flags) {
		std::array flags_array{ flags... };
		auto iter = std::ranges::find_if(flags_array, [=](auto flag) { return static_cast<bool>(supported & flag); });
		return iter != flags_array.end() ? *iter : *--iter;
	}


	struct VulkanDispatcher : GlobalState<VulkanDispatcher> {
		VulkanDispatcher();
	};

	vk::Instance createInstance(const vk::ApplicationInfo& app_info, std::span<const char* const> layers = {}, std::span<const char* const> extensions = {});

	std::pair<vk::Device, Queues> createDeviceWithQueues(vk::PhysicalDevice physical_device, vk::SurfaceKHR optional_surface, std::span<const char* const> extensions = {});

	vk::DebugUtilsMessengerEXT createDebugMessenger(vk::Instance instance);

	void recreateSwapchain(
		vk::PhysicalDevice physical_device, 
		vk::Device device, 
		const Queues& queues, 
		vk::SurfaceKHR surface, 
		vk::Format format, 
		vk::PresentModeKHR present_mode,
		vk::Extent2D resolution, 
		std::uint32_t image_count, 
		Swapchain& swapchain
	);

	std::pair<vk::Viewport, vk::Rect2D> createViewport(vk::Extent2D resolution, float min_depth = 0.f, float max_depth = 1.f);

	vk::ImageSubresourceLayers toImageLayers(const vk::ImageSubresourceRange& range);

	void waitForFences(vk::Device device, std::span<const vk::Fence> fences, bool reset = true, bool wait_all = true);
	void waitForFences(vk::Device device, const vk::Fence fence, bool reset = true) { waitForFences(device, std::span<const vk::Fence>{{ fence }}, reset); }

	vk::SamplerCreateInfo simpleSampler(vk::Filter filter = vk::Filter::eNearest, vk::SamplerMipmapMode mipmap_mode = vk::SamplerMipmapMode::eNearest, vk::SamplerAddressMode address_mode = vk::SamplerAddressMode::eRepeat);
}