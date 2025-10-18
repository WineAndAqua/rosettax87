#include "offset_finder.hpp"

auto OffsetFinder::setDefaultOffsets() -> void {
	// These are the default offsets for the rosetta runtime that matches MD5 hash: d7819a04355cd77ff24031800a985c13

	offsetExportsFetch_ = 0xFA8C; // Just before fetching 'exports' structure pointed by X19 and just after checking rosetta runtime version from header
	//               LDR X8, [X19]  - X19 'exports' structure address
	//               MOV X9, #1
	//               MOVK X9, #0x6A00,LSL#32
	//               MOVK X9, #1,LSL#48
	//               CMP X8, X9  // if [X19] < 0x16A0000000001
	//               B.CS <error version flow>
	// 62 06 40 F9 - LDR X2, [X19,#8]  <--- halt point for override X19 with new 'export' structure address
	// 63 12 40 B9 - LDR W3, [X19,#0x10]

	offsetSvcCallEntry_ = 0x1998; // The entry point of a function that trigger BSD syscall 'mmap'
	// B0 18 80 D2 - MOV X16, #197 <--- start for mmap wrapper
	// 01 10 00 D4 - SVC 0x80
	// E1 37 9F 9A - CSET X1, CS
	// offset: 0x19A4:
	// C0 03 5F D6 - RET <--- end of function
	offsetSvcCallRet_ = offsetSvcCallEntry_ + 0xC; // The return point of the above function
}

auto OffsetFinder::determineOffsets() -> void {
	// byte patterns in hex for the functions we need to find.
	const std::vector<unsigned char> exportsFetch = {0x62, 0x06, 0x40, 0xF9, 0x63, 0x12, 0x40, 0xB9 };
	const std::vector<unsigned char> svcCall = { 0xB0, 0x18, 0x80, 0xD2, 0x01, 0x10, 0x00, 0xD4, 0xE1, 0x37, 0x9F, 0x9A, 0xC0, 0x03, 0x5F, 0xD6 };
	// For svc_call we need to check where this bitpattern starts in the code and also where it ends (we can just add 0xC to the start to get the end)

	// Load rosetta runtime into an ifstream
	std::ifstream file{"/usr/libexec/rosetta/runtime", std::ios::binary};

	// Check if we were successfully able to load the file, if not abort and use default offsets
	if (!file) {
		printf("Problem accessing rosetta runtime to determine offsets automatically.\nFalling back to macOS 26.0 defaults (This WILL crash your app if they are not correct!)\n");
		return;
	}

	// Determine size of rosetta runtime file
	file.seekg(0, std::ios::end);
	std::streampos size = file.tellg();
	file.seekg(0, std::ios::beg);

	// Set our buffer to the size of the file
	std::vector<unsigned char> buffer(size);

	// read into the buffer
	if (!file.read(reinterpret_cast<char *>(buffer.data()), size)) {
		printf("Problem reading rosetta runtime to determine offsets automatically.\nFalling back to macOS 26.0 defaults (This WILL crash your app if they are not correct!)\n");
		return;
	}

	// Do the search and store the results
	std::vector<std::uint64_t> results;
	for (const auto offset : { exportsFetch, svcCall }) {
		const std::boyer_moore_searcher searcher(offset.begin(), offset.end());
		const auto it = std::search(buffer.begin(), buffer.end(), searcher);
		if (it == buffer.end()) {
			std::cout << "Offset not found in rosetta runtime binary\n";
			results.push_back(-1);
		} else {
			results.push_back((std::uint64_t)std::distance(buffer.begin(), it));
		}
	}

	// If we've stored -1 in any offset, error out and fall back to non-accelerated x87 handles.
	if ((int)results[0] <= -1 || (int)results[1] <= -1) {
		printf("Problem searching rosetta runtime to determine offsets automatically.\nFalling back to macOS 26 defaults (This WILL crash your app if they are not correct!)\n");
		return;
	}

	// Set the offsets to the results that we've found now that we know they're "correct".
	offsetExportsFetch_ = results[0];
	offsetSvcCallEntry_ = results[1];
	offsetSvcCallRet_ = offsetSvcCallEntry_ + 0xC;

	printf("Found rosetta runtime offsets successfully! offset_exports_fetch=%llx offset_svc_call_entry=%llx offset_svc_call_ret=%llx\n", offsetExportsFetch_, offsetSvcCallEntry_, offsetSvcCallRet_);
}
