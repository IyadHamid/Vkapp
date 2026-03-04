module;

#include "log.h"

#include <vulkan/vk_platform.h>         // VKAPI_*
#include <vulkan/vulkan_hpp_macros.hpp> // VULKAN_HPP_DEFAULT_DISPATCH_LOADER_*

module vkapp;

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

using namespace vkapp;

// https://github.com/KhronosGroup/Vulkan-Hpp/blob/ee361cf520a3344decddf0b5c6c0d74fe50079eb/samples/EnableValidationWithCallback/EnableValidationWithCallback.cpp#L54
VKAPI_ATTR vk::Bool32 VKAPI_CALL debugMessageFunc(
	vk::DebugUtilsMessageSeverityFlagBitsEXT message_severity,
	vk::DebugUtilsMessageTypeFlagsEXT message_types,
	vk::DebugUtilsMessengerCallbackDataEXT const* callback_data,
	[[maybe_unused]] void* user_data
) {
	using enum vk::DebugUtilsMessageSeverityFlagBitsEXT;

	auto log_level = [&] {
		switch (message_severity) {
		case eError:
			return spdlog::level::err;
		case eWarning:
			return spdlog::level::warn;
		case eInfo:
			return spdlog::level::info;
		case eVerbose:
			return spdlog::level::debug;
		}
		throw std::runtime_error("unknown severity");
	}();
	std::string final_log;
	auto log = [&final_log]<typename... Args>(std::format_string<Args...> fmt, Args&&... args) { 
		std::format_to(std::back_inserter(final_log), fmt, std::forward<Args>(args)...); 
		final_log.append("\n"); 
	};


	auto message_types_str = vk::to_string(message_types);
	auto message_types_min = std::string_view(message_types_str).substr(2, message_types_str.size() - 4);
	log("{} Error: {} [{:#x}]", message_types_min, callback_data->pMessageIdName, callback_data->messageIdNumber);

	if (message_severity == eError or message_severity == eWarning) {
		std::string_view message(callback_data->pMessage);
		auto short_message_sep = message.rfind(" | ") + 2;
		auto long_message_sep = message.rfind('\n');
		auto short_message = message.substr(short_message_sep + 1, long_message_sep - short_message_sep - 1);
		auto long_message = message.substr(long_message_sep + 1);
		log("\t{}", short_message);
		log("\t{}", long_message);
	}
	else {
		log("\t{}", callback_data->pMessage);
	}


	if (auto count = callback_data->queueLabelCount; 0 < count) {
		log("\tQueue Labels:");
		for (std::uint32_t i = 0; i < count; i++)
			log("\t\t[{}] name: \"{}\"", i, callback_data->pQueueLabels[i].pLabelName);
	}
	if (auto count = callback_data->cmdBufLabelCount; 0 < count) {
		log("\tCommandBuffer Labels {}:", count);
		for (std::uint32_t i = 0; i < count; i++)
			log("\t\t[{}] name: \"{}\"", i, callback_data->pCmdBufLabels[i].pLabelName);
	}
	if (auto count = callback_data->objectCount; 0 < count) {
		log("\tObjects {}:", count);
		for (std::uint32_t i = 0; i < count; i++) {
			if (auto name = callback_data->pObjects[i].pObjectName)
				log("\t\t[{}] {:#x}, type: {}, name: \"{}\"", i, callback_data->pObjects[i].objectHandle, vk::to_string(callback_data->pObjects[i].objectType), name);
			else
				log("\t\t[{}] {:#x}, type: {}", i, callback_data->pObjects[i].objectHandle, vk::to_string(callback_data->pObjects[i].objectType));
		}
	}
	log("Stacktrace (uncaught exceptions '{}'):", std::uncaught_exceptions() == 0);
	for (auto&& s : currentRelevantStacktrace(1))
		log("\t{}({})", s.source_file(), s.source_line());

	spdlog::log(log_level, final_log);
	return false;
}

VulkanDispatcher::VulkanDispatcher() {
	VULKAN_HPP_DEFAULT_DISPATCHER.init();
}

vk::Instance vkapp::createInstance(const vk::ApplicationInfo& app_info, std::span<const char* const> layers, std::span<const char* const> extensions) {
	auto enabled_layers = gather<LayerDetails>(layers);
	auto enabled_extensions = gather<ExtensionDetails>(extensions);

	SPDLOG_INFO("enabling instance layers: {:n}", enabled_layers);
	SPDLOG_INFO("enabling instance extensions: {:n}", enabled_extensions);

	auto instance = vk::createInstance(vk::InstanceCreateInfo({}, &app_info, enabled_layers, enabled_extensions));
	SPDLOG_INFO("created instance");

	VULKAN_HPP_DEFAULT_DISPATCHER.init(instance);

	return instance;
}

std::pair<vk::Device, Queues> vkapp::createDeviceWithQueues(vk::PhysicalDevice physical_device, vk::SurfaceKHR optional_surface, std::span<const char* const> extensions) {
	check(physical_device, "physical device not valid");
	Queues queues;

	auto is_graphics = [&](const QueueFamilyInfo& info) -> bool { return static_cast<bool>(info.second.queueFlags & (vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute)); };
	auto is_transfer = [&](const QueueFamilyInfo& info) -> bool { return static_cast<bool>(info.second.queueFlags & (vk::QueueFlagBits::eTransfer)) and not is_graphics(info); };
	auto is_presents = [&](const QueueFamilyInfo& info) -> bool { return not optional_surface or physical_device.getSurfaceSupportKHR(info.first, optional_surface); };

	auto g_family_opt = findQueueFamilyIndex(physical_device, is_graphics);
	auto t_family_opt = findQueueFamilyIndex(physical_device, is_transfer);
	auto p_family_opt = findQueueFamilyIndex(physical_device, is_presents);

	check(g_family_opt.has_value(), "unable to find graphics queue family index");
	check(p_family_opt.has_value(), "unable to find present queue family index");
	if (not t_family_opt) {
		t_family_opt = g_family_opt;
		// TODO maybe smarter? check count
		//queues.queue_indices[Queues::Transfer] = 1u;
	}

	queues.family_indices = { *g_family_opt, *t_family_opt, *p_family_opt };

	std::unordered_map<std::uint32_t, int> unique_families{};
	for (auto index : queues.family_indices)
		unique_families[index]++;

	std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
	queue_create_infos.reserve(unique_families.size());

	std::array<float, Queues::Count> queue_priorities;
	std::ranges::fill(queue_priorities, 1.0f);

	auto queue_priorities_ptr = queue_priorities.data();
	for (auto [index, count] : unique_families) {
		//check(count <= physical_device.getQueueFamilyProperties()[index].queueCount, "queue family does not support enough queues");
		count = 1;

		assert(queue_priorities_ptr < queue_priorities.data() + queue_priorities.size());
		queue_create_infos.emplace_back(vk::DeviceQueueCreateInfo(
			{},
			index,
			count, queue_priorities_ptr
		));
		queue_priorities_ptr += count;
	}

	SPDLOG_INFO("enabling device extensions: {:n}", extensions);

	// TODO[refactor]: expose at function call
	vk::StructureChain create_info{
		vk::DeviceCreateInfo({}, queue_create_infos, {}, extensions),
		vk::PhysicalDeviceDynamicRenderingFeatures(true),
		vk::PhysicalDeviceShaderObjectFeaturesEXT(true),
		vk::PhysicalDeviceSynchronization2Features(true),
		vk::PhysicalDeviceDescriptorIndexingFeatures()
			.setShaderSampledImageArrayNonUniformIndexing(true)
			.setRuntimeDescriptorArray(true)
			.setDescriptorBindingVariableDescriptorCount(true)
			.setDescriptorBindingPartiallyBound(true)
			.setDescriptorBindingSampledImageUpdateAfterBind(true)
			.setDescriptorBindingUniformBufferUpdateAfterBind(true)
			.setDescriptorBindingStorageImageUpdateAfterBind(true),
		vk::PhysicalDeviceBufferDeviceAddressFeatures(true),
	};
	auto device = physical_device.createDevice(create_info.get<vk::DeviceCreateInfo>());

	std::ranges::transform(queues.family_indices, queues.queue_indices, queues.queues.begin(),
		[&](auto family, auto queue) { return device.getQueue(family, queue); }
	);

	VULKAN_HPP_DEFAULT_DISPATCHER.init(device);

	return std::pair(device, std::move(queues));
}

vk::DebugUtilsMessengerEXT vkapp::createDebugMessenger(vk::Instance instance) {
	using enum vk::DebugUtilsMessageSeverityFlagBitsEXT;
	using enum vk::DebugUtilsMessageTypeFlagBitsEXT;
	return instance.createDebugUtilsMessengerEXT(vk::DebugUtilsMessengerCreateInfoEXT(
		{},
		eError | eWarning /*| eInfo | eVerbose*/,
		eGeneral | eValidation | ePerformance,
		debugMessageFunc
	));
}

void vkapp::recreateSwapchain(
	vk::PhysicalDevice physical_device, 
	vk::Device device, 
	const Queues& queues, 
	vk::SurfaceKHR surface, 
	vk::Format format, 
	vk::PresentModeKHR present_mode,
	vk::Extent2D resolution, 
	std::uint32_t image_count, 
	Swapchain& swapchain
) {
	// TODO[get the supported VkFormats]

	auto formats = physical_device.getSurfaceFormatsKHR(surface);
	assert(not formats.empty());
	swapchain.format = (formats[0].format != vk::Format::eUndefined) ? formats[0].format : vk::Format::eB8G8R8A8Unorm;
	// assert(std::ranges::contains(physical_device.getSurfaceFormatsKHR(surface), format));
	// swapchain.format = format;

	auto capabilities = physical_device.getSurfaceCapabilitiesKHR(surface);
	auto clamp = [](const vk::Extent2D& x, const vk::Extent2D& min, const vk::Extent2D& max) { return vk::Extent2D(std::clamp(x.width, min.width, max.width), std::clamp(x.height, min.height, max.height)); };
	// TODO[add swapchain recreation]
	auto extent = (capabilities.currentExtent.width == std::numeric_limits<std::uint32_t>::max())
		? clamp(resolution, capabilities.minImageExtent, capabilities.maxImageExtent)
		: capabilities.currentExtent;

	auto pre_transform = [&] {
		using enum vk::SurfaceTransformFlagBitsKHR;
		return findSupported(capabilities.supportedTransforms, eIdentity, capabilities.currentTransform);
	}();

	auto composite_alpha = [&] {
		using enum vk::CompositeAlphaFlagBitsKHR;
		return findSupported(capabilities.supportedCompositeAlpha, ePreMultiplied, ePostMultiplied, eInherit, eOpaque);
	}();

	auto max_image_count = capabilities.maxImageCount > 0 ? capabilities.maxImageCount : std::numeric_limits<std::uint32_t>::max();
	check(capabilities.minImageCount <= image_count and image_count <= max_image_count, "bad image count");
	vk::SwapchainCreateInfoKHR create_info(
		{},
		surface,
		image_count,
		swapchain.format,
		vk::ColorSpaceKHR::eSrgbNonlinear, // TODO[find format manually]
		extent,
		1,
		vk::ImageUsageFlagBits::eColorAttachment,
		vk::SharingMode::eExclusive,
		{},
		pre_transform,
		composite_alpha,
		present_mode,
		true,
		swapchain.swapchain
	);

	check(queues.family_indices[Queues::Graphics] == queues.family_indices[Queues::Presents], "differing graphics and present queues");
	//std::array queue_family_indicies{ queues.family_indices[Queues::Graphics], queues.family_indices[Queues::Present] };
	//if (device.indices.present != device.indices.graphics) {
	//	// either explicitly transfer image or use concurrent sharing
	//	// TODO[vulkan]: keep as exclusive
	//	create_info.imageSharingMode = vk::SharingMode::eConcurrent;
	//	create_info.setQueueFamilyIndices(queue_family_indicies);
	//}

	swapchain.swapchain = device.createSwapchainKHR(create_info);

	vk::ImageViewCreateInfo image_view_create_info({}, nullptr, vk::ImageViewType::e2D, swapchain.format, {}, vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
	auto create_image_view = [&](vk::Image image) { return device.createImageView(image_view_create_info.setImage(image)); };

	swapchain.images = device.getSwapchainImagesKHR(swapchain.swapchain);
	swapchain.image_views = std::vector(std::from_range,
		swapchain.images | std::views::transform(create_image_view)
	);
}


std::pair<vk::Viewport, vk::Rect2D> vkapp::createViewport(vk::Extent2D resolution, float min_depth, float max_depth) {
	return {
		vk::Viewport{
			0.0f, 0.0f,
			static_cast<float>(resolution.width), static_cast<float>(resolution.height),
			min_depth, max_depth
		},
		vk::Rect2D{ { 0, 0 }, resolution }
	};
}

vk::ImageSubresourceLayers vkapp::toImageLayers(const vk::ImageSubresourceRange& range) {
	return vk::ImageSubresourceLayers(range.aspectMask, range.baseMipLevel, range.baseArrayLayer, range.layerCount);
}

void vkapp::waitForFences(vk::Device device, std::span<const vk::Fence> fences, bool reset, bool wait_all) {
	if (device.waitForFences(fences, wait_all, std::numeric_limits<std::uint64_t>::max()) != vk::Result::eSuccess)
		SPDLOG_WARN("wait for fences failed");
	if (reset)
		device.resetFences(fences);
}

vk::SamplerCreateInfo vkapp::simpleSampler(vk::Filter filter, vk::SamplerMipmapMode mipmap_mode, vk::SamplerAddressMode address_mode) {
	return vk::SamplerCreateInfo({}, filter, filter, mipmap_mode, address_mode, address_mode, address_mode).setMaxLod(vk::LodClampNone);
}
