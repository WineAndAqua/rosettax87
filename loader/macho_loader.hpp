#pragma once

#include <filesystem>
#include <functional>
#include <mach-o/loader.h>
#include <vector>

struct MachoLoader {
	auto open(std::filesystem::path const &path) -> bool;
	auto mach_header() const -> mach_header_64 *;
	auto image_size() const -> size_t;
	auto get_section(const char *segment, const char *section) -> section_64 *;
	auto for_each_segment(std::function<void(segment_command_64 *segm)>) -> void;

	std::vector<uint8_t> buffer_;
};
