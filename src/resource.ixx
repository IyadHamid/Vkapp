module;

#include "log.h"

export module vkapp:resource;

import std;
import vulkan;

import :functional;
import :device_owner;

namespace vkapp {
	export struct MemoryUsage {
		vk::PipelineStageFlags2 stage;
		vk::AccessFlags2 access;
	};
	export namespace memory_usage {
		using Stage = vk::PipelineStageFlagBits2;
		using Access = vk::AccessFlagBits2;
		using Layout = vk::ImageLayout;
		export {
			constexpr MemoryUsage undefined    { Stage::eTopOfPipe     , Access::eNone          };
			constexpr MemoryUsage graphics_read{ Stage::eFragmentShader, Access::eShaderRead    };
			constexpr MemoryUsage compute_read { Stage::eComputeShader , Access::eShaderRead    };
			constexpr MemoryUsage compute_write{ Stage::eComputeShader , Access::eShaderWrite   };
			constexpr MemoryUsage transfer_src { Stage::eTransfer      , Access::eTransferRead  };
			constexpr MemoryUsage transfer_dst { Stage::eTransfer      , Access::eTransferWrite };
			constexpr MemoryUsage host_read    { Stage::eHost          , Access::eHostRead      };
			constexpr MemoryUsage host_write   { Stage::eHost          , Access::eHostWrite     };
		}
	}
	export struct ImageUsage {
		vk::PipelineStageFlags2 stage;
		vk::AccessFlags2 access;
		vk::ImageLayout layout;

		[[nodiscard]] bool undefined() const { return layout == vk::ImageLayout::eUndefined; }
	};
	export namespace image_usage {
		using Stage = vk::PipelineStageFlagBits2;
		using Access = vk::AccessFlagBits2;
		using Layout = vk::ImageLayout;
		export {
			constexpr ImageUsage undefined         { Stage::eTopOfPipe            , Access::eNone                , Layout::eUndefined              };
			constexpr ImageUsage color_attachment  { Stage::eColorAttachmentOutput, Access::eColorAttachmentWrite, Layout::eColorAttachmentOptimal };
			constexpr ImageUsage present           { Stage::eBottomOfPipe         , Access::eNone                , Layout::ePresentSrcKHR          };
			constexpr ImageUsage swapchain_acquired{ Stage::eColorAttachmentOutput, Access::eNone                , Layout::eUndefined              };
			constexpr ImageUsage graphics_read     { Stage::eFragmentShader       , Access::eShaderRead          , Layout::eShaderReadOnlyOptimal  };
			constexpr ImageUsage compute_read_only { Stage::eComputeShader        , Access::eShaderRead          , Layout::eShaderReadOnlyOptimal  };
			constexpr ImageUsage compute_read      { Stage::eComputeShader        , Access::eShaderRead          , Layout::eGeneral                };
			constexpr ImageUsage compute_write     { Stage::eComputeShader        , Access::eShaderWrite         , Layout::eGeneral                };
			constexpr ImageUsage transfer_src      { Stage::eTransfer             , Access::eTransferRead        , Layout::eTransferSrcOptimal     };
			constexpr ImageUsage transfer_dst      { Stage::eTransfer             , Access::eTransferWrite       , Layout::eTransferDstOptimal     };
		}
	}
}

namespace vkapp {
	export struct BufferRange {
		vk::DeviceSize offset;
		vk::DeviceSize size;
	};

	export [[nodiscard]] vk::MemoryBarrier2 createBarrier(const MemoryUsage& src, const MemoryUsage& dst) {
		return vk::MemoryBarrier2{
			src.stage, src.access,
			dst.stage, dst.access
		};
	}

	export [[nodiscard]] vk::BufferMemoryBarrier2 createBarrier(
		vk::Buffer buffer,
		BufferRange range,
		const MemoryUsage& src,
		const MemoryUsage& dst,
		std::uint32_t src_family_index = vk::QueueFamilyIgnored,
		std::uint32_t dst_family_index = vk::QueueFamilyIgnored
	) {
		return vk::BufferMemoryBarrier2{
			src.stage, src.access,
			dst.stage, dst.access,
			src_family_index, dst_family_index,
			buffer, range.offset, range.size
		};
	}

	export [[nodiscard]] vk::ImageMemoryBarrier2 createBarrier(
		vk::Image image,
		const vk::ImageSubresourceRange& range,
		const ImageUsage& src,
		const ImageUsage& dst,
		std::uint32_t src_family_index = vk::QueueFamilyIgnored,
		std::uint32_t dst_family_index = vk::QueueFamilyIgnored
	) {
		return vk::ImageMemoryBarrier2{
			src.stage, src.access,
			dst.stage, dst.access,
			src.layout, dst.layout,
			src_family_index, dst_family_index,
			image, range
		};
	}



	vk::ImageAspectFlags getImageAspect(vk::Format format) {
		using enum vk::ImageAspectFlagBits;
		// TODO[update]: use vulkan_format_traits.hpp
		switch (format) {
		case vk::Format::eD16Unorm:
		case vk::Format::eX8D24UnormPack32:
		case vk::Format::eD32Sfloat:
			return eDepth;
		case vk::Format::eD16UnormS8Uint:
		case vk::Format::eD24UnormS8Uint:
		case vk::Format::eD32SfloatS8Uint:
			return eDepth | eStencil;
		case vk::Format::eS8Uint:
			return eStencil;
		default:
			return eColor;
		}
	}
}

namespace vkapp {
	export struct Image {
		struct Extent {
			vk::Extent3D resolution;
			std::uint32_t mip_levels = 1u;
			std::uint32_t array_layers = 1u;

			Extent() = default;
			Extent(vk::Extent3D resolution, std::uint32_t mip_levels = 1u, std::uint32_t array_layers = 1u) : 
				resolution(resolution), mip_levels{ mip_levels }, array_layers{ array_layers }
			{}
			Extent(vk::Extent2D resolution, std::uint32_t mip_levels = 1u, std::uint32_t array_layers = 1u) : 
				resolution(resolution, 1u), mip_levels{ mip_levels }, array_layers{ array_layers }
			{}

			std::pair<vk::ImageType, vk::ImageViewType> getImageType() const {
				auto image_type = vk::ImageType::e1D;
				if (resolution.height != 1u)
					image_type = vk::ImageType::e2D;
				else if (resolution.depth != 1u)
					image_type = vk::ImageType::e3D;

				auto image_view_type = vk::ImageViewType::e3D;
				if (image_type == vk::ImageType::e1D)
					image_view_type = (array_layers == 1u ? vk::ImageViewType::e1D : vk::ImageViewType::e1DArray);
				else if (image_type == vk::ImageType::e2D)
					image_view_type = (array_layers == 1u ? vk::ImageViewType::e2D : vk::ImageViewType::e2DArray);
				return { image_type, image_view_type };
			};
		};

		Extent extent;
		vk::Format format;

		vk::Image image;
		vma::Allocation allocation;
		vk::ImageView full_view;


		Image() = default;
		Image(
			DeviceOwner owner, 
			Extent extent, 
			vk::Format format = vk::Format::eR8G8B8A8Unorm,
			vk::ImageUsageFlags usage_flags = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
			vma::AllocationCreateFlags allocation_flags = {}, 
			vma::MemoryUsage memory_usage = vma::MemoryUsage::eAuto
		) : extent{ extent }, format{ format } {
			auto image_tiling = allocation_flags & vma::AllocationCreateFlagBits::eHostAccessSequentialWrite or allocation_flags & vma::AllocationCreateFlagBits::eHostAccessRandom ? vk::ImageTiling::eLinear : vk::ImageTiling::eOptimal;
			auto [image_type, image_view_type] = extent.getImageType();
			std::tie(image, allocation) = owner.allocator.createImage(
				vk::ImageCreateInfo(
					{},
					image_type, format,
					extent.resolution, extent.mip_levels, extent.array_layers,
					vk::SampleCountFlagBits::e1,
					image_tiling,
					usage_flags
				),
				vma::AllocationCreateInfo(allocation_flags, memory_usage)
			);
			full_view = createImageView(owner, image_view_type);
		}

		[[nodiscard]] vk::ImageSubresourceRange getRange() const {
			return vk::ImageSubresourceRange(getImageAspect(format), 0u, extent.mip_levels, 0u, extent.array_layers);
		}
		[[nodiscard]] vk::ImageSubresourceRange getLevelRange(std::uint32_t mip_level) const {
			return vk::ImageSubresourceRange(getImageAspect(format), mip_level, 1u, 0u, extent.array_layers);
		}
		[[nodiscard]] vk::ImageSubresourceRange getLayerRange(std::uint32_t array_layer) const {
			return vk::ImageSubresourceRange(getImageAspect(format), 0u, extent.mip_levels, array_layer, 1u);
		}

		[[nodiscard]] vk::ImageView createImageView(DeviceOwner owner, vk::ImageViewType type, vk::ImageSubresourceRange range, vk::ComponentMapping mapping = {}) const {
			return owner.device.createImageView(vk::ImageViewCreateInfo({}, image, type, format, mapping, range));
		}
		[[nodiscard]] vk::ImageView createImageView(DeviceOwner owner, vk::ImageViewType type) const {
			return createImageView(owner, type, getRange());
		}
		[[nodiscard]] vk::ImageView createImageView(DeviceOwner owner) const {
			return createImageView(owner, extent.getImageType().second);
		}
		[[nodiscard]] vk::ImageView createLayerImageView(DeviceOwner owner, std::uint32_t array_layer = 0u) const {
			assert(extent.array_layers > array_layer && "array layer out of bounds");
			assert(extent.array_layers > 1u && "image type is not an array");
			auto image_view_type = extent.getImageType().first == vk::ImageType::e2D ? vk::ImageViewType::e2D : vk::ImageViewType::e1D;
			return owner.device.createImageView(vk::ImageViewCreateInfo({}, image, image_view_type, format, {}, getLayerRange(array_layer)));
		}

		[[nodiscard]] vk::ImageMemoryBarrier2 createBarrier(const ImageUsage& src, const ImageUsage& dst, std::uint32_t src_family_index = vk::QueueFamilyIgnored, std::uint32_t dst_family_index = vk::QueueFamilyIgnored) const {
			return vkapp::createBarrier(image, getRange(), src, dst, src_family_index, dst_family_index);
		}

		vk::RenderingAttachmentInfo createAttachment(vk::ClearValue clear_value = {}, vk::ImageView image_view = nullptr) {
			if (image_view == nullptr)
				image_view = full_view;
			const auto image_layout = getImageAspect(format) == vk::ImageAspectFlagBits::eColor
				? vk::ImageLayout::eColorAttachmentOptimal
				: vk::ImageLayout::eDepthStencilAttachmentOptimal;
			return vk::RenderingAttachmentInfo(
				image_view,
				image_layout,
				vk::ResolveModeFlagBits::eNone,
				{},
				vk::ImageLayout::eUndefined,
				vk::AttachmentLoadOp::eClear,
				vk::AttachmentStoreOp::eStore,
				clear_value
			);
		}
	};
	export template<>
	struct Destroyer<Image> {
		static void operator()(DeviceOwner owner, Image& image) {
			if (image.full_view)
				owner.device.destroyImageView(image.full_view);
			if (image.image)
				owner.allocator.destroyImage(image.image, image.allocation);
			image = {};
		}
	};

	export struct Buffer {
		vk::Buffer buffer;
		vma::Allocation allocation;
		vk::DeviceAddress address;
		vk::DeviceSize size;
		void* mapped_data = nullptr;

		Buffer() = default;
		Buffer(
			DeviceOwner owner,
			vk::DeviceSize size,
			vk::BufferUsageFlags usage_flags = vk::BufferUsageFlagBits::eStorageBuffer,
			vma::AllocationCreateFlags alloc_flags = {},
			vma::MemoryUsage usage = vma::MemoryUsage::eAuto
		) : size{ size } {
			vma::AllocationInfo alloc_info;
			std::tie(buffer, allocation) = owner.allocator.createBuffer(
				vk::BufferCreateInfo({}, size, usage_flags | vk::BufferUsageFlagBits::eShaderDeviceAddress),
				vma::AllocationCreateInfo(alloc_flags, usage),
				alloc_info
			);
			address = owner.device.getBufferAddress(vk::BufferDeviceAddressInfo(buffer));
			if (alloc_info.pMappedData)
				mapped_data = alloc_info.pMappedData;
		}

		template <typename T>
		Buffer(DeviceOwner owner, std::span<const T> data, vk::BufferUsageFlags usage_flags = vk::BufferUsageFlagBits::eStorageBuffer) : 
			Buffer(owner, data.size_bytes(), usage_flags, vma::AllocationCreateFlagBits::eHostAccessSequentialWrite)
		{
			owner.allocator.copyMemoryToAllocation(data.data(), allocation, 0, data.size_bytes());
		}
		template <typename T>
		static Buffer unmapped(
			DeviceOwner owner,
			vk::DeviceSize size,
			vk::BufferUsageFlags usage_flags = vk::BufferUsageFlagBits::eStorageBuffer,
			vma::AllocationCreateFlags alloc_flags = {},
			vma::MemoryUsage usage = vma::MemoryUsage::eAuto
		) {
			return Buffer(owner, size * sizeof(T), usage_flags, alloc_flags, usage);
		}

		template <typename T>
		static Buffer mapped(
			DeviceOwner owner,
			std::size_t size,
			vk::BufferUsageFlags usage_flags = vk::BufferUsageFlagBits::eStorageBuffer,
			vma::AllocationCreateFlags other_alloc_flags = {},
			vma::MemoryUsage usage = vma::MemoryUsage::eAuto
		) {
			return Buffer(
				owner, 
				size * sizeof(T), 
				usage_flags, 
				other_alloc_flags | vma::AllocationCreateFlagBits::eHostAccessSequentialWrite | vma::AllocationCreateFlagBits::eMapped, 
				usage
			);
		}

		[[nodiscard]] BufferRange getRange() const { return { 0, size }; }

		[[nodiscard]] vk::BufferMemoryBarrier2 createBarrier(const MemoryUsage& src, const MemoryUsage& dst, std::uint32_t src_family_index = vk::QueueFamilyIgnored, std::uint32_t dst_family_index = vk::QueueFamilyIgnored) const {
			return vkapp::createBarrier(buffer, getRange(), src, dst, src_family_index, dst_family_index);
		}

		template <typename T>
		[[nodiscard]] std::span<T> mapped(std::size_t offset = 0, std::size_t count = std::dynamic_extent) { 
			return std::span(reinterpret_cast<T*>(static_cast<std::byte*>(mapped_data) + offset), std::min((size - offset) / sizeof(T), count)); 
		}

		void flush(DeviceOwner owner) { owner.allocator.flushAllocation(allocation, 0, size); }

	}; 
	export template<>
	struct Destroyer<Buffer> {
		static void operator()(DeviceOwner owner, Buffer& buffer) {
			if (buffer.buffer)
				owner.allocator.destroyBuffer(buffer.buffer, buffer.allocation);
			buffer = {};
		}
	};

	export template <class... Barriers>
		requires ((
			std::convertible_to<Barriers, vk::MemoryBarrier2> or
			std::convertible_to<Barriers, vk::BufferMemoryBarrier2> or
			std::convertible_to<Barriers, vk::ImageMemoryBarrier2>
		) and ...)
	void pipelineBarriers(vk::CommandBuffer cmd, const Barriers&... barriers) {
		std::vector<vk::MemoryBarrier2> memory_barriers;
		std::vector<vk::BufferMemoryBarrier2> buffer_memory_barriers;
		std::vector<vk::ImageMemoryBarrier2> image_memory_barriers;
		([&](const auto& barrier) {
			if constexpr (std::convertible_to<Barriers, vk::MemoryBarrier2>)
				memory_barriers.push_back(barrier);
			if constexpr (std::convertible_to<Barriers, vk::BufferMemoryBarrier2>)
				buffer_memory_barriers.push_back(barrier);
			if constexpr (std::convertible_to<Barriers, vk::ImageMemoryBarrier2>)
				image_memory_barriers.push_back(barrier);
		}(barriers), ...);
		cmd.pipelineBarrier2(vk::DependencyInfo(vk::DependencyFlagBits::eByRegion, memory_barriers, buffer_memory_barriers, image_memory_barriers));
	}
}

namespace vkapp {
	export struct ImageAndRange { vk::Image image; vk::ImageSubresourceRange range; };

	export void sequentialTransformation(
		vk::CommandBuffer cmd,
		const ImageUsage& src,
		const ImageUsage& inter,
		const ImageUsage& dst,
		std::ranges::input_range auto&& imgs,
		std::invocable<vk::CommandBuffer, const ImageAndRange&, const ImageAndRange&> auto&& f
	)
		requires std::same_as<std::ranges::range_value_t<decltype(imgs)>, ImageAndRange>
	{
		ImageAndRange final;
		for (const auto& [from, to] : imgs | std::views::pairwise) {
			auto src_barrier = createBarrier(from.image, from.range, src, inter);
			cmd.pipelineBarrier2(vk::DependencyInfo(vk::DependencyFlagBits::eByRegion, {}, {}, src_barrier));
			std::invoke(f, cmd, from, to);

			if (not dst.undefined()) {
				auto dst_barrier = createBarrier(from.image, from.range, inter, dst);
				cmd.pipelineBarrier2(vk::DependencyInfo(vk::DependencyFlagBits::eByRegion, {}, {}, dst_barrier));
			}
			final.image = to.image;
			final.range = to.range;
		}
		auto dst_barrier = createBarrier(final.image, final.range, src, dst.undefined() ? inter : dst);
		cmd.pipelineBarrier2(vk::DependencyInfo(vk::DependencyFlagBits::eByRegion, {}, {}, dst_barrier));
	}
}



namespace vkapp {
	export void setDebugName(vk::Device device, const Image& image, zstring_view name) {
		setDebugName(device, image.image, std::format("{}.image", name));
		setDebugName(device, image.full_view, std::format("{}.full_view", name));
	}
	export void setDebugName(vk::Device device, const Buffer& buffer, zstring_view name) {
		setDebugName(device, buffer.buffer, std::format("{}.buffer", name));
	}
}