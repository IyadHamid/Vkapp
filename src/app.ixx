module;

#include <SDL3/SDL.h>

#include "log.h"

export module vkapp:app;

import std;
import vulkan;
import vk_mem_alloc;
import glm;

import :utils;
import :io;
import :functional;
import :shader;
import :glm_compat;
import :device_owner;
import :resource;

namespace vkapp {

	export class DescriptorManager {
	public:

		struct DescriptorsInfo {
			vk::DescriptorPool pool;
			vk::DescriptorSetLayout layout;
			vk::DescriptorSet set;
		};

		DeviceOwner owner;
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
			info->pool = owner->createDescriptorPool(vk::DescriptorPoolCreateInfo(
				vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet | vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind, 
				max_sets, 
				pool_sizes
			));

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
			info->layout = owner->createDescriptorSetLayout(create_info.get<vk::DescriptorSetLayoutCreateInfo>());
			info->set = owner->allocateDescriptorSets(vk::DescriptorSetAllocateInfo(info->pool, info->layout))[0];
			owner.nameAs("DescriptorManager.pool", info->pool);
			owner.nameAs("DescriptorManager.layout", info->layout);
			owner.nameAs("DescriptorManager.set", info->set);
		}

		DescriptorsInfo* operator->() const { return info.operator->(); }

		void update(std::uint32_t index, vk::ImageView view) const {
			// Storage Image
			vk::DescriptorImageInfo image_info(nullptr, view, vk::ImageLayout::eGeneral);
			owner->updateDescriptorSets(
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

	template <typename T, std::size_t N>
	struct DefaultArrayFor {
		std::array<T, N> value;
		constexpr DefaultArrayFor(auto&&... xs) : value{ xs... } {}
		constexpr operator std::array<T, N>() { return value; }
		template <std::size_t M>
		constexpr operator std::array<T, M>() { return {}; }
	};

	export template <typename Arg>
	struct ComputeSource {
		using arg_type = Arg;
		zstring_view file;
		zstring_view stage;
	};

	export template <typename Arg, std::size_t NumStages = 2, std::size_t NumAttachments = 1>
	struct GraphicsSource {
		using arg_type = Arg;
		static constexpr std::size_t num_stages = NumStages;
		static constexpr std::size_t num_attachments = NumAttachments;
		zstring_view file;
		std::array<zstring_view, num_stages> stages = DefaultArrayFor<zstring_view, 2>{ "vertexMain", "pixelMain" };
		std::array<vk::Format, num_attachments> color = DefaultArrayFor<vk::Format, 1>{ vk::Format::eB8G8R8A8Unorm };
		vk::Format depth = vk::Format::eUndefined;
		vk::Format stencil = vk::Format::eUndefined; // TODO[logic]: batch with depth (depth_stencil format)

		std::optional<vk::PipelineVertexInputStateCreateInfo> vertex_input = {};
		std::optional<vk::PipelineInputAssemblyStateCreateInfo> input_assembly = {};
		std::optional<vk::PipelineTessellationStateCreateInfo> tessellation = {};
		std::optional<vk::PipelineViewportStateCreateInfo> viewport = {};
		std::optional<vk::PipelineRasterizationStateCreateInfo> rasterization = {};
		std::optional<vk::PipelineMultisampleStateCreateInfo> multisample = {};
		std::optional<vk::PipelineDepthStencilStateCreateInfo> depth_stencil = {};
		std::optional<vk::PipelineColorBlendStateCreateInfo> color_blend = {};

		auto states(this auto&& self) {
			return std::forward_as_tuple(
				self.vertex_input, self.input_assembly, self.tessellation, self.viewport,
				self.rasterization, self.multisample, self.depth_stencil, self.color_blend
			);
		}
		template <typename CreateInfo>
		GraphicsSource& set(const CreateInfo& create_info) {
			std::get<std::optional<CreateInfo>&>(states()) = create_info;
			return *this;
		}
	};


	export template <typename Arg, vk::PipelineBindPoint BindPoint>
		requires (std::is_empty_v<Arg> or sizeof(Arg) % 4 == 0)
	class Pipeline {
	public:
		using arg_type = Arg;
		static constexpr bool has_arg = not std::is_empty_v<arg_type>;

	private:
		vk::PipelineLayout layout;
		vk::DescriptorSet set;
		Unique<vk::Pipeline> pipeline;

	public:
		Pipeline(vk::PipelineLayout layout, vk::DescriptorSet set, Unique<vk::Pipeline>&& pipeline) :
			layout{ layout },
			set{ set },
			pipeline{ std::move(pipeline) }
		{}

		void bind(vk::CommandBuffer cmd, const arg_type& arg = {}) {
			cmd.bindPipeline(BindPoint, *pipeline);
			cmd.bindDescriptorSets(BindPoint, layout, 0, set, {});
			if constexpr (has_arg)
				cmd.pushConstants(layout, vk::ShaderStageFlagBits::eAll, 0, sizeof(arg_type), &arg);
		}
	};

	export template <typename Arg>
	using ComputePipeline = Pipeline<Arg, vk::PipelineBindPoint::eCompute>;
	export template <typename Arg>
	using GraphicsPipeline = Pipeline<Arg, vk::PipelineBindPoint::eGraphics>;

	export template <typename Arg>
	class Kernel : public ComputePipeline<Arg> {
	public:
		using arg_type = Arg;

		Kernel(ComputePipeline<Arg>&& pipeline) : ComputePipeline<Arg>(std::move(pipeline)) {}

		void operator()(vk::CommandBuffer cmd, vk::Extent3D extent, const arg_type& arg = {}) {
			ComputePipeline<Arg>::bind(cmd, arg);
			cmd.dispatch(extent.width, extent.height, extent.depth);
		}
	};

	export class ScreenTriangle {
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

namespace vkapp {
	class DebugInstance {
		vk::Instance instance;
		vk::DebugUtilsMessengerEXT debug_messenger;
	public:
		DebugInstance(vk::Instance instance) :
			instance{ instance }, debug_messenger{ createDebugMessenger(instance) }
		{}
		~DebugInstance() {
			instance.destroy(debug_messenger);
			instance.destroy();
		}

		vk::Instance operator*() const { return instance; }
		operator vk::Instance() const { return instance; }
		const vk::Instance* operator->() const { return &instance; }
	};
}

// Application
namespace vkapp {
	export class Application : VulkanDispatcher {
	public:
		Window window;

		DebugInstance instance;

		// Window surface
		WindowSurface surface;
		vk::Extent2D resolution;

		// Device
		vk::PhysicalDevice physical_device;
		Queues queues;
		UniqueDeviceOwner unique_owner;

		DescriptorManager desc_manager;
		ShaderSession shader_compiler;

		// Swapchain
		vk::Format swapchain_format;
		vk::PresentModeKHR present_mode;
		Unique<Swapchain> swapchain;

		struct Immediate {
			vk::CommandPool pool;
			vk::CommandBuffer buffer;
			vk::Fence fence;
		};
		Unique<Immediate> immediate;

		// Current frame
		static constexpr std::uint32_t max_frame_count = 3; // TODO[logic]: write assertions
		std::uint32_t frame_count = 0;
		std::uint32_t frame_index = 0;
		std::uint32_t swapchain_image_index = 0;
		Image swapchain_image_stub = {};

		// Pipline management
		std::unordered_map<std::uint64_t, Unique<vk::PipelineLayout>> pipeline_layouts = {};

		// Application management
		std::atomic_bool do_quit = false;
		std::move_only_function<void(Application&, vk::Extent2D)> on_resize = nullptr;

		Application(
			zstring_view app_name,
			std::uint32_t app_version,
			vk::Extent2D resolution = { 800, 600 },
			std::span<const char* const> shader_search_paths = {},
			std::span<const MacroDesc> shader_macros = {},
			std::uint32_t frame_count = 3u,
			vk::Format swapchain_format = vk::Format::eB8G8R8A8Unorm,
			vk::PresentModeKHR present_mode = vk::PresentModeKHR::eFifo
		);
		Application(const Application&) = delete;
		Application(Application&&) = delete; // TODO add by moving window

		~Application();

#pragma region queues
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
#pragma endregion

#pragma region immediate
		void immediateWait() { waitForFences(owner(), immediate->fence, false); }

		void immediateSubmit(std::invocable<vk::CommandBuffer> auto&& f) {
			waitForFences(owner(), immediate->fence);

			owner()->resetCommandPool(immediate->pool);
			immediate->buffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
			std::invoke(f, immediate->buffer);
			immediate->buffer.end();
			graphics()->submit(vk::SubmitInfo({}, {}, { immediate->buffer }, {}), immediate->fence);
		}
#pragma endregion

		[[nodiscard]] DeviceOwner owner() const { return unique_owner; }

		[[nodiscard]] vk::Extent2D getResolution() const { return resolution; }

#pragma region vulkan utilities
	public:
		[[nodiscard]] auto createCommandPool(Queues::QueuesEnum family_queue = Queues::Graphics, vk::CommandPoolCreateFlags flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer) const {
			return owner()->createCommandPool(vk::CommandPoolCreateInfo(flags, queues.family_indices[family_queue]));
		}
		[[nodiscard]] auto semaphoreCreator() const {
			return [&] { return owner()->createSemaphore(vk::SemaphoreCreateInfo{}); };
		}
		[[nodiscard]] auto fenceCreator(bool signaled = true) const {
			return [&, signaled] { return owner()->createFence(vk::FenceCreateInfo{ signaled ? vk::FenceCreateFlags(vk::FenceCreateFlagBits::eSignaled) : vk::FenceCreateFlags{} }); };
		}
#pragma endregion
#pragma region swapchain
	public:
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
#pragma region pipeline management
	public:
		vk::PipelineLayout findPipelineLayout(std::size_t arg_size) {
			auto index = static_cast<std::uint64_t>(arg_size);
			if (auto it = pipeline_layouts.find(index); it != pipeline_layouts.end())
				return *it->second;
			// TODO[C++26]: std::optional range
			auto ranges = std::vector(
				arg_size < 4 ? 0 : 1, 
				vk::PushConstantRange(vk::ShaderStageFlagBits::eAll, 0, arg_size)
			);
			return *pipeline_layouts.emplace(index, owner().makeUnique(
				owner()->createPipelineLayout(vk::PipelineLayoutCreateInfo({}, desc_manager->layout, ranges))
			)).first->second;
		}

		template <class... Args>
		std::tuple<ComputePipeline<Args>...> createComputePipelines(const ComputeSource<Args>&... sources) {
			constexpr auto count = sizeof...(Args);
			// TODO[C++26]: rewrite with structured binding parameter pack
			std::array arg_sizes = { sizeof(Args)... };
			return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
				std::array shader_infos = { shader_compiler.load(owner(), sources.file, {{ sources.stage }})... };
				std::array<vk::ComputePipelineCreateInfo, count> create_infos;
				for (auto&& [create, info, size] : std::views::zip(create_infos, shader_infos, arg_sizes)) {
					create = vk::ComputePipelineCreateInfo(
						{},
						info.first[0].get(),
						findPipelineLayout(size)
					);
				}
				auto pipelines = *owner()->createComputePipelines({}, create_infos);

				return std::tuple{ Pipeline<Args, vk::PipelineBindPoint::eCompute>(
					findPipelineLayout(sizeof(Args)), desc_manager->set, owner().makeUnique(pipelines[Is])
				)... };
			}(std::index_sequence_for<Args...>{});
		}

		template <class... Args, std::size_t... NumStages, std::size_t... NumAttachments>
		std::tuple<GraphicsPipeline<Args>...> createGraphicsPipelines(const GraphicsSource<Args, NumStages, NumAttachments>&... sources) {
			constexpr auto count = sizeof...(Args);
			// TODO[C++26]: rewrite with structured binding parameter pack

			auto get_dynamic_states = [](auto& source) {
				return []<class... CreateInfos>(std::tuple<const std::optional<CreateInfos>&...> states) {
					// TODO[C++26]: std::views::concat;
					std::vector<vk::DynamicState> dynamic_states{};
					([&] {
						if (not std::get<const std::optional<CreateInfos>&>(states)) {
							const auto& states = dynamicStatesOf<CreateInfos>();
							dynamic_states.append_range(states);
						}
					}(), ...);

					return dynamic_states;
				}(source.states());
			};
			auto opt_ptr = [](auto&& opt) { return opt ? std::to_address(opt) : nullptr; };

			return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
				std::array shader_infos = { shader_compiler.load(owner(), sources.file, sources.stages)...};
				std::tuple<std::array<vk::PipelineShaderStageCreateInfo, NumStages>...> shader_stages = {
					[&]<std::size_t... Js>(std::size_t i, std::index_sequence<Js...>) {
						return std::array{ shader_infos[i].first[Js].get()... };
					}(Is, std::make_index_sequence<NumStages>{})...
				};
				std::array chain_infos{ 
					vk::StructureChain{
						vk::GraphicsPipelineCreateInfo(
							{},
							std::get<Is>(shader_stages),
							opt_ptr(std::get<0>(sources.states())),
							opt_ptr(std::get<1>(sources.states())),
							opt_ptr(std::get<2>(sources.states())),
							opt_ptr(std::get<3>(sources.states())),
							opt_ptr(std::get<4>(sources.states())),
							opt_ptr(std::get<5>(sources.states())),
							opt_ptr(std::get<6>(sources.states())),
							opt_ptr(std::get<7>(sources.states())),
							{},
							findPipelineLayout(sizeof(Args))
						),
						vk::PipelineRenderingCreateInfo(
							{},
							sources.color,
							sources.depth,
							sources.stencil
						)
					}...
				};
				std::array dynamic_states{
					fillDynamicStates(chain_infos[Is].get())...
				};
				std::vector create_infos(std::from_range, chain_infos | std::views::transform([](auto& chain) { return chain.get(); }));
				auto pipelines = *owner()->createGraphicsPipelines({}, create_infos);

				return std::tuple{ Pipeline<Args, vk::PipelineBindPoint::eGraphics>(
					findPipelineLayout(sizeof(Args)), desc_manager->set, owner().makeUnique(pipelines[Is])
				)... };
			}(std::index_sequence_for<Args...>{});
		}
		
		template <class... Args>
		std::tuple<vkapp::Kernel<Args>...> kernels(const ComputeSource<Args>&... sources) {
			auto pipelines = createComputePipelines(sources...);
			// TODO[C++26]: rewrite with structured binding parameter pack
			return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
				return std::tuple{ Kernel<Args>(std::move(std::get<Is>(pipelines)))...};
			}(std::index_sequence_for<Args...>{});
		}
		

#pragma endregion
#pragma region application management
	public:
		std::generator<SDL_Event> poll();

		void quit() { do_quit = true; }
		[[nodiscard]] bool running() { return not do_quit; }
#pragma endregion
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