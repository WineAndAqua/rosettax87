#pragma once

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

struct OffsetFinder {
	auto set_default_offsets() -> void;
	auto determine_offsets() -> void;

	std::uint64_t offset_loop_copy_func;
	std::uint64_t offset_svc_call_entry;
	std::uint64_t offset_svc_call_ret;
};
