module;

#include <slang.h>
#include <slang-com-ptr.h>

#include "log.h"

export module vkapp:shader;

import vulkan;

import :utils;
import :functional;

export namespace vkapp {
	using MacroDesc = slang::PreprocessorMacroDesc;

	class ShaderSession {
		Slang::ComPtr<slang::IGlobalSession> global_session;
		Slang::ComPtr<slang::ISession> session;


	public:
		ShaderSession(std::span<const char* const> search_paths, std::span<const MacroDesc> macros);

	private:
		std::vector<std::pair<Slang::ComPtr<slang::IBlob>, SlangStage>> compile(zstring_view name, std::span<zstring_view> entry_point_names);

		vk::ShaderStageFlagBits stageCompat(SlangStage stage) {
			using enum vk::ShaderStageFlagBits;
			switch (stage) {
			case SLANG_STAGE_NONE:           return {};
			case SLANG_STAGE_VERTEX:         return eVertex;
			case SLANG_STAGE_HULL:           return eTessellationControl;
			case SLANG_STAGE_DOMAIN:         return eTessellationEvaluation;
			case SLANG_STAGE_GEOMETRY:       return eGeometry;
			case SLANG_STAGE_FRAGMENT:       return eFragment;
			case SLANG_STAGE_COMPUTE:        return eCompute;
			case SLANG_STAGE_RAY_GENERATION: return eRaygenKHR;
			case SLANG_STAGE_INTERSECTION:   return eIntersectionKHR;
			case SLANG_STAGE_ANY_HIT:        return eAnyHitKHR;
			case SLANG_STAGE_CLOSEST_HIT:    return eClosestHitKHR;
			case SLANG_STAGE_MISS:           return eMissKHR;
			case SLANG_STAGE_CALLABLE:       return eCallableKHR;
			case SLANG_STAGE_MESH:           return eMeshEXT;
			case SLANG_STAGE_AMPLIFICATION:  return eTaskEXT;
			default: fail("unsupported stage");
			}
		}
	public:
		struct EntryPointDescription {
			zstring_view name;
			std::span<const vk::DescriptorSetLayout> set_layouts = {};
			std::span<const vk::PushConstantRange> push_constant_ranges = {};
		};
		std::vector<vk::ShaderEXT> load(vk::Device device, zstring_view name, std::span<EntryPointDescription> entry_point_descs);
	};

}
