#pragma once

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

struct OffsetFinder {
	auto setDefaultOffsets() -> void;
	auto determineOffsets() -> void;

	std::uint64_t offsetExportsFetch_;
	std::uint64_t offsetSvcCallEntry_;
	std::uint64_t offsetSvcCallRet_;
};
