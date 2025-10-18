#pragma once

#include <iostream>

struct OffsetFinder {
	auto setDefaultOffsets() -> void;
	auto determineOffsets() -> bool;

	std::uint64_t offsetExportsFetch_;
	std::uint64_t offsetSvcCallEntry_;
	std::uint64_t offsetSvcCallRet_;
};
