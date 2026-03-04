module;

#include <SDL3/SDL.h>

#include "log.h"

export module vkapp:app;

import std;
import vulkan;
import vk_mem_alloc_hpp;
import glm;

import :utils;
import :io;
import :functional;
import :shader;
import :glm_compat;
import :device_owner;
import :resource;

// Application
export namespace vkapp {
	class Application : VulkanDispatcher {
	public:
		Window window;

		vk::Instance instance;

		vk::DebugUtilsMessengerEXT debug_messenger;

		vk::SurfaceKHR surface;
		vk::Extent2D resolution;

		vk::PhysicalDevice physical_device;
		vk::Device device;
		Queues queues;

		vma::Allocator allocator;

		vk::Format swapchain_format;
		vk::PresentModeKHR present_mode;
		Swapchain swapchain;

		struct Immediate {
			vk::CommandPool pool;
			vk::CommandBuffer buffer;
			vk::Fence fence;
		} immediate = {};

		static constexpr std::uint32_t max_frame_count = 3; // TODO[logic]: write assertions
		std::uint32_t frame_count = 0;
		std::uint32_t frame_index = 0;
		std::uint32_t swapchain_image_index = 0;
		Image swapchain_image_stub = {};

		std::atomic_bool do_quit = false;
		std::move_only_function<void(Application&, vk::Extent2D)> on_resize = nullptr;

		Application(
			zstring_view app_name, 
			std::uint32_t app_version, 
			vk::Extent2D resolution = { 800, 600 }, 
			std::uint32_t frame_count = 3u, 
			vk::Format swapchain_format = vk::Format::eB8G8R8A8Unorm,
			vk::PresentModeKHR present_mode = vk::PresentModeKHR::eFifo
		);
		Application(const Application&) = delete;
		Application(Application&&) = delete; // TODO add by moving window

		~Application();

		struct Queue {
			std::uint32_t family;
			vk::Queue queue;
			explicit(false) operator std::uint32_t() const { return family; }
			explicit(false) operator vk::Queue() const { return queue; }
			vk::Queue* operator->() { return &queue; }
		};
		Queue queue(Queues::QueuesEnum q) { return { queues.family_indices[q], queues.queues[q] }; }
		Queue graphics() { return queue(Queues::Graphics); }
		Queue transfer() { return queue(Queues::Transfer); }
		Queue presents() { return queue(Queues::Presents); }

		void immediateWait() { waitForFences(device, immediate.fence); }

		void immediateSubmit(std::invocable<vk::CommandBuffer> auto&& f) {
			immediateWait();
			device.resetCommandPool(immediate.pool);
			immediate.buffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
			std::invoke(f, immediate.buffer);
			immediate.buffer.end();
			graphics()->submit(vk::SubmitInfo({}, {}, { immediate.buffer }, {}), immediate.fence);
		}

		[[nodiscard]] DeviceOwner owner() const { return { device, allocator }; }

		[[nodiscard]] vk::Extent2D getResolution() const { return resolution; }

#pragma region vulkan utilities
		[[nodiscard]] auto createCommandPool(Queues::QueuesEnum family_queue = Queues::Graphics, vk::CommandPoolCreateFlags flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer) const {
			return device.createCommandPool(vk::CommandPoolCreateInfo(flags, queues.family_indices[family_queue]));
		}
		[[nodiscard]] auto semaphoreCreator() const {
			return [&] { return device.createSemaphore(vk::SemaphoreCreateInfo{}); };
		}
		[[nodiscard]] auto fenceCreator(bool signaled = true) const {
			return [&, signaled] { return device.createFence(vk::FenceCreateInfo{ signaled ? vk::FenceCreateFlags(vk::FenceCreateFlagBits::eSignaled) : vk::FenceCreateFlags{} }); };
		}
#pragma endregion
#pragma region swapchain
		void setFrameCount(std::uint32_t count) {
			assert(1 <= count and count <= max_frame_count);
			frame_count = count;
			resize();
		}
		void setOnResize(auto&& callback) {
			on_resize = std::forward<decltype(callback)>(callback);
			if (on_resize)
				on_resize(*this, resolution);
		}

		const Image& acquireFrame(vk::Semaphore semaphore);
		void present(vk::Semaphore wait);

		void resize();

		glm::vec2 resolutionAsFloat() const { return glm::vec2{ resolution.width, resolution.height }; }
#pragma endregion

		std::generator<SDL_Event> poll();

		void quit() { do_quit = true; }
		[[nodiscard]] bool running() { return not do_quit; }
	};

}


export namespace vkapp {
	class ImGuiState {
	public:
		explicit ImGuiState(Application& app, vk::Format depth_format = vk::Format::eUndefined, vk::Format stencil_format = vk::Format::eUndefined);
		~ImGuiState();

		bool processEvent(const SDL_Event& event);
		void newFrame();
		void render(vk::CommandBuffer cmd);
		// TODO[logic]: ImGui_ImplVulkan_SetMinImageCount
	};
}

export namespace vkapp {

	class DescriptorManager {
	public:

		struct DescriptorsInfo {
			vk::DescriptorPool pool;
			vk::DescriptorSetLayout layout;
			vk::DescriptorSet set;
		};

		DeviceOwner owner; // TODO: replace with allocators
		std::uint32_t max_sets;
		std::uint32_t max_count;
		Unique<DescriptorsInfo> info;

		explicit DescriptorManager(DeviceOwner owner, std::uint32_t max_sets = 32u, std::uint32_t max_count = 256u) :
			owner{ owner },
			max_sets{ max_sets },
			max_count{ max_count },
			info{ owner.makeUnique(DescriptorsInfo{}) }
		{
			std::array pool_sizes = {
				vk::DescriptorPoolSize(vk::DescriptorType::eStorageImage, max_sets * max_count),
				vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, max_sets * max_count),
				vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, max_sets * max_count),
			};
			info->pool = owner.device.createDescriptorPool(vk::DescriptorPoolCreateInfo(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet | vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind, max_sets, pool_sizes));

			std::array descriptor_set_layout_bindings = {
				vk::DescriptorSetLayoutBinding(0u, vk::DescriptorType::eStorageImage, max_count, vk::ShaderStageFlagBits::eAll),
				vk::DescriptorSetLayoutBinding(1u, vk::DescriptorType::eCombinedImageSampler, max_count, vk::ShaderStageFlagBits::eAll),
				vk::DescriptorSetLayoutBinding(2u, vk::DescriptorType::eUniformBuffer, max_count, vk::ShaderStageFlagBits::eAll),
			};
			std::array descriptor_binding_flags = {
				vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eUpdateAfterBind,
				vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eUpdateAfterBind,
				vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eUpdateAfterBind,
			};
			vk::StructureChain create_info{
				vk::DescriptorSetLayoutCreateInfo(vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool, descriptor_set_layout_bindings),
				vk::DescriptorSetLayoutBindingFlagsCreateInfo(descriptor_binding_flags),
			};
			info->layout = owner.device.createDescriptorSetLayout(create_info.get<vk::DescriptorSetLayoutCreateInfo>());
			info->set = owner.device.allocateDescriptorSets(vk::DescriptorSetAllocateInfo(info->pool, info->layout))[0];
			owner.nameAs("DescriptorManager.pool", info->pool);
			owner.nameAs("DescriptorManager.layout", info->layout);
			owner.nameAs("DescriptorManager.set", info->set);
		}

		DescriptorsInfo* operator->() const { return info.operator->(); }

		void update(std::uint32_t index, vk::ImageView view) const {
			// Storage Image
			vk::DescriptorImageInfo image_info(nullptr, view, vk::ImageLayout::eGeneral);
			owner.device.updateDescriptorSets(
				vk::WriteDescriptorSet(info->set, 0u, index, 1u, vk::DescriptorType::eStorageImage, &image_info, {}, {}),
				{}
			);
		}
		void update(std::uint32_t index, vk::ImageView view, vk::Sampler sampler) const {
			// Combined Image Sampler
			vk::DescriptorImageInfo image_info(sampler, view, vk::ImageLayout::eShaderReadOnlyOptimal);
			owner.device.updateDescriptorSets(
				vk::WriteDescriptorSet(info->set, 1u, index, 1u, vk::DescriptorType::eCombinedImageSampler, &image_info, {}, {}),
				{}
			);
		}
		void update(std::uint32_t index, vk::Buffer buffer) const {
			// Uniform Buffer
			vk::DescriptorBufferInfo buffer_info(buffer, 0u, vk::WholeSize);
			owner.device.updateDescriptorSets(
				vk::WriteDescriptorSet(info->set, 2u, index, 1u, vk::DescriptorType::eUniformBuffer, {}, &buffer_info, {}, {}),
				{}
			);
		}
		void updateRWImage(std::uint32_t index, vk::ImageView view, vk::Sampler sampler) const { update(index, view); update(index, view, sampler); }
	};

	class PipelineLayout {
		static vk::PipelineBindPoint findBindPoint(vk::ShaderStageFlags stage) {
			using enum vk::ShaderStageFlagBits;
			constexpr auto all_raytracing_khr = eRaygenKHR | eAnyHitKHR | eClosestHitKHR | eMissKHR | eIntersectionKHR | eCallableKHR;
			constexpr auto all_raytracing_nv = eRaygenNV | eAnyHitNV | eClosestHitNV | eMissNV | eIntersectionNV | eCallableNV;
			if (stage & eCompute)
				return vk::PipelineBindPoint::eCompute;
			if (stage & all_raytracing_khr)
				return vk::PipelineBindPoint::eRayTracingKHR;
			if (stage & all_raytracing_nv)
				return vk::PipelineBindPoint::eRayTracingNV;

			return vk::PipelineBindPoint::eGraphics; // guess graphics, others not supported
		}
	public:
		DescriptorManager& manager;
		vk::PipelineBindPoint bind_point;
		vk::ShaderStageFlags stages;
		vk::PushConstantRange range;
		Unique<vk::PipelineLayout> layout;

		auto ranges() const { return std::span(&range, range.size < 4 ? 0u: 1u); }
		auto rangesProxy() const { return vk::ArrayProxyNoTemporaries(ranges().size(), ranges().data()); }

		PipelineLayout(DescriptorManager& manager, std::size_t push_constant_size, vk::ShaderStageFlags stages = vk::ShaderStageFlagBits::eAllGraphics) :
			manager{ manager },
			bind_point{ findBindPoint(stages) },
			stages{ stages },
			range(stages, 0, push_constant_size),
			layout{ manager.owner.makeUnique(manager.owner.device.createPipelineLayout(vk::PipelineLayoutCreateInfo({}, manager->layout, rangesProxy()))) }
		{
		}

		auto createShaders(ShaderSession& session, zstring_view source, std::span<const zstring_view> entry_points) const {
			std::vector entry_point_descs(std::from_range, entry_points | std::views::transform([&](auto&& name) {
				return vkapp::ShaderSession::EntryPointDescription{ name, std::span(&manager->layout, 1), ranges() };
			}));
			return session.load(manager.owner.device, source, entry_point_descs);
		}

		void bind(vk::CommandBuffer cmd, const auto& push_constant) const {
			cmd.bindDescriptorSets(bind_point, *layout, 0, manager->set, {});
			if constexpr (not std::is_empty_v<std::remove_cvref_t<decltype(push_constant)>>) {
				assert(sizeof(push_constant) == range.size);
				cmd.pushConstants(*layout, stages, 0, sizeof(push_constant), &push_constant);
			}
		}
	};

	template <typename Arg>
	class Kernel {
	public:
		// TODO[reflection]?: generate argument type
		using argument_t = Arg;
	private:
		PipelineLayout layout;
		Unique<vk::ShaderEXT> shader;
	public:
		Kernel(DescriptorManager& manager, ShaderSession& session, zstring_view source, zstring_view name) : 
			layout(manager, sizeof(argument_t), vk::ShaderStageFlagBits::eCompute),
			shader{ manager.owner.makeUnique(layout.createShaders(session, source, {{ name }})[0])}
		{
		}

		void operator()(vk::CommandBuffer cmd, vk::Extent3D extent, const Arg& arg) {
			cmd.bindShadersEXT({ vk::ShaderStageFlagBits::eCompute }, *shader);
			layout.bind(cmd, arg);
			cmd.dispatch(extent.width, extent.height, extent.depth);
		}
	};

	class ScreenTriangle {
		struct Vertex { glm::vec2 pos; };

		static auto getAttributes() {
			static const auto attributes = vkapp::makeVertexInputAttribute<Vertex>();
			return attributes;
		}
		static constexpr std::array vertices{
			Vertex{ { -1.f, -1.f } },
			Vertex{ {  3.f, -1.f } },
			Vertex{ { -1.f,  3.f } },
		};

		Unique<Buffer> vertex_buffer;
	public:
		explicit ScreenTriangle(DeviceOwner owner) :
			vertex_buffer(owner.makeUniqueWithName("ScreenTriangle.vertex_buffer", Buffer(owner, std::span<const Vertex>(vertices), vk::BufferUsageFlagBits::eVertexBuffer)))
		{
		}

		void draw(vk::CommandBuffer cmd) {
			// Input assembly settings
			cmd.setPrimitiveTopology(vk::PrimitiveTopology::eTriangleList);
			cmd.setPrimitiveRestartEnable(false);

			// Rasterization settings
			cmd.setRasterizerDiscardEnable(false);
			cmd.setCullMode(vk::CullModeFlagBits::eNone);
			cmd.setFrontFace(vk::FrontFace::eClockwise);
			cmd.setPolygonModeEXT(vk::PolygonMode::eFill);
			cmd.setDepthBiasEnable(false);

			// Multisample settings
			cmd.setRasterizationSamplesEXT(vk::SampleCountFlagBits::e1);
			cmd.setSampleMaskEXT(vk::SampleCountFlagBits::e1, vk::SampleMask{ 1 });
			cmd.setAlphaToCoverageEnableEXT(false);

			// Depth stencil settings
			cmd.setDepthWriteEnable(false);
			cmd.setDepthTestEnable(false);
			cmd.setStencilTestEnable(false);

			// Color blend settings
			cmd.setColorBlendEnableEXT(0, false);
			cmd.setColorWriteMaskEXT(0, vk::FlagTraits<vk::ColorComponentFlagBits>::allFlags);

			cmd.setVertexInputEXT(vk::VertexInputBindingDescription2EXT(0, sizeof(Vertex), vk::VertexInputRate::eVertex, 1u), getAttributes());
			cmd.bindVertexBuffers(0, { vertex_buffer->buffer }, { 0 });

			cmd.draw(3u, 1u, 0u, 0u);
		}

		void draw(vk::CommandBuffer cmd, vk::Viewport viewport, vk::Rect2D scissor, bool flip_y = false) {
			if (flip_y) {
				viewport.y = viewport.height;
				viewport.height = -viewport.height;
			}
			cmd.setViewportWithCount(viewport);
			cmd.setScissorWithCount(scissor);

			draw(cmd);
		}
	};
}