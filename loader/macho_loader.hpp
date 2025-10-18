#pragma once

#include <filesystem>
#include <functional>
#include <mach-o/loader.h>
#include <vector>

struct MachoLoader {
	auto open(std::filesystem::path const &path) -> bool;
	auto machHeader() const -> mach_header_64 *;
	auto imageSize() const -> size_t;
	auto getSection(const char *segment, const char *section) -> section_64 *;
	auto forEachSegment(std::function<void(segment_command_64 *segm)>) -> void;

	std::vector<uint8_t> buffer_;
};
