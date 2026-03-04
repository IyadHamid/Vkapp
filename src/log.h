#pragma once

#define VKAPP_NAME "Vulkan App Engine"
#define VKAPP_VERSION 0

#include <cassert>
#include <stacktrace>

#define SPDLOG_USE_STD_FORMAT
#include <spdlog/spdlog.h> 

namespace vkapp {
	inline void check(bool condition, const char* message) {
		if (condition)
			return;
		SPDLOG_ERROR("{}\n{}", message, std::stacktrace::current());
		throw std::runtime_error(message);
	}
	inline void check(int return_value, const char* message) { check(return_value >= 0, message); }
	[[noreturn]] inline void fail(const char* message) { check(false, message); }
}