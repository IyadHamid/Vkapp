module;

#include "log.h"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>


module vkapp;

using namespace vkapp;

constexpr auto api_version = vk::ApiVersion14; // TODO[refactor]: expose

Application::Application(
	zstring_view app_name, 
	std::uint32_t app_version, 
	vk::Extent2D resolution,
	std::span<const char* const> shader_search_paths,
	std::span<const MacroDesc> shader_macros,
	std::uint32_t frame_count, 
	vk::Format swapchain_format, 
	vk::PresentModeKHR present_mode
) :
	window(app_name, resolution),
	instance{ [&] {
		std::vector<const char*> extensions(std::from_range, window.getInstanceExtensions());
		extensions.push_back(vk::EXTDebugUtilsExtensionName);
		auto instance = createInstance(
			{ app_name, app_version, (VKAPP_NAME), (VKAPP_VERSION), api_version },
			{},
			extensions
		);
		return DebugInstance(instance);
	}() },
	surface{ window.createSurface(instance) },
	resolution{ resolution },
	swapchain_format{ swapchain_format },
	present_mode{ present_mode },
	physical_device{ [&] {
		const auto& physical_devices = instance->enumeratePhysicalDevices();

		// TODO[algo]: select physical device
		auto it = std::ranges::find_if(physical_devices, [](auto pd) {
			return pd.getProperties().apiVersion >= api_version and pd.getProperties().deviceType == vk::PhysicalDeviceType::eDiscreteGpu;
		});
		check(it != physical_devices.end(), "no valid physical devices");
		return *it;
	}() },
	queues{}, // initalize with owner
	unique_owner{ [&] {
		std::vector<const char*> extensions{
			vk::KHRSwapchainExtensionName,
			vk::KHRMaintenance5ExtensionName,
			vk::EXTExtendedDynamicState3ExtensionName,
			vk::EXTColorWriteEnableExtensionName,
			vk::EXTVertexInputDynamicStateExtensionName,
		};
		vk::Device device;
		std::tie(device, queues) = createDeviceWithQueues(physical_device, surface, extensions);

		vma::VulkanFunctions funcs = vma::functionsFromDispatcher();
		auto allocator =  vma::createAllocator(vma::AllocatorCreateInfo(
			vma::AllocatorCreateFlagBits::eBufferDeviceAddress,
			physical_device,
			device,
			vk::DeviceSize{},
			nullptr,
			nullptr,
			nullptr,
			&funcs,
			instance,
			api_version
		));
		return UniqueDeviceOwner(device, allocator);
	}() },
	desc_manager(owner()),
	shader_compiler(shader_search_paths, shader_macros),
	immediate{ [&] {
		auto pool = createCommandPool(Queues::Graphics, {});
		return owner().makeUnique(Immediate(
			owner().nameAs("app.immediate.pool", pool),
			owner().nameAs("app.immediate.buffer", owner()->allocateCommandBuffers(vk::CommandBufferAllocateInfo(pool, vk::CommandBufferLevel::ePrimary, 1))[0]),
			owner().nameAs("app.immediate.fence", fenceCreator()())
		));
	}() }
{
	SPDLOG_INFO("created application \"{}\" {}", app_name, app_version);
	SPDLOG_INFO("created device on \"{}\"", std::string_view(physical_device.getProperties().deviceName));

	setFrameCount(frame_count);
}

Application::~Application() {
	SPDLOG_INFO("destroyed application");
}

const Image& Application::acquireFrame(vk::Semaphore semaphore) {
	auto acquire_next = [&] { return owner()->acquireNextImageKHR(swapchain->swapchain, std::numeric_limits<uint64_t>::max(), semaphore, nullptr); };

	vk::Result result;
	std::tie(result, swapchain_image_index) = acquire_next();
	if (result == vk::Result::eErrorOutOfDateKHR) {
		resize();
		std::tie(result, swapchain_image_index) = acquire_next();
		check(result == vk::Result::eSuccess, "unable to acquire frame");
	}
	else 
		check(result == vk::Result::eSuccess || result == vk::Result::eSuboptimalKHR, "unable to acquire frame");

	swapchain_image_stub.extent = Image::Extent(vk::Extent3D(resolution, 1u));
	swapchain_image_stub.format = swapchain->format;
	swapchain_image_stub.image = swapchain->images[swapchain_image_index];
	swapchain_image_stub.full_view = swapchain->image_views[swapchain_image_index];

	return swapchain_image_stub;
}

void Application::present(vk::Semaphore wait) {
	auto _ = queues.queues[Queues::Presents].presentKHR(vk::PresentInfoKHR(
		{ wait },
		swapchain->swapchain,
		swapchain_image_index,
		{}
	));
	if (++frame_index >= frame_count)
		frame_index = 0;
}

void Application::resize() {
	owner()->waitIdle();
	resolution = window.getWindowSize();
	swapchain = owner().makeUnique(Swapchain());
	recreateSwapchain(physical_device, owner(), queues, surface, swapchain_format, present_mode, resolution, frame_count, *swapchain);
	owner().nameAs("swapchain.images", swapchain->images);
	owner().nameAs("swapchain.image_views", swapchain->image_views);

	if (on_resize)
		on_resize(*this, resolution);
}

std::generator<SDL_Event> Application::poll() {
	bool do_resize = false;
	for (SDL_Event event : window.poll()) {
		switch (event.type) {
		case SDL_EVENT_QUIT:
			quit();
			break;
		case SDL_EVENT_WINDOW_RESIZED:
		case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
			do_resize = true;
			break;
		default:
			co_yield event;
			break;
		}
	}
	if (do_resize)
		resize();
}


ImGuiState::ImGuiState(Application& app, vk::Format depth_format, vk::Format stencil_format) {
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

	// Setup Platform/Renderer backends
	ImGui_ImplSDL3_InitForVulkan(app.window.window);
	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.ApiVersion = api_version;
	init_info.Instance = *app.instance;
	init_info.PhysicalDevice = app.physical_device;
	init_info.Device = app.owner().device;
	init_info.QueueFamily = app.queues.family_indices[Queues::Graphics];
	init_info.Queue = app.queues.queues[Queues::Graphics];

	init_info.MinImageCount = 2;
	init_info.ImageCount = app.frame_count;
	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	init_info.DescriptorPoolSize = 2;

	std::array color_attachment_formats = { app.swapchain->format };
	init_info.UseDynamicRendering = true;
	init_info.PipelineRenderingCreateInfo = vk::PipelineRenderingCreateInfo({}, color_attachment_formats, depth_format, stencil_format);

	static auto imguiCheck = [](VkResult result) { check(result == std::to_underlying(vk::Result::eSuccess), "ImGui Vulkan error"); };
	// VULKAN_HPP_DEFAULT_DISPATCHER
	// init_info.Allocator = YOUR_ALLOCATOR;
	// vk::AllocationCallbacks allocator = {};
	// init_info.CheckVkResultFn = imguiCheck;
	ImGui_ImplVulkan_Init(&init_info);

	ImGui_ImplVulkan_CreateFontsTexture();
	// (your code submit a queue)
	ImGui_ImplVulkan_DestroyFontsTexture();
}

ImGuiState::~ImGuiState() {
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();

}

bool ImGuiState::processEvent(const SDL_Event& event) {
	ImGui_ImplSDL3_ProcessEvent(&event);

	if (const auto& io = ImGui::GetIO(); io.WantCaptureKeyboard or io.WantCaptureMouse)
		return true;
	return false;
}

void ImGuiState::newFrame() {
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();
}

void ImGuiState::render(vk::CommandBuffer cmd) {
	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}