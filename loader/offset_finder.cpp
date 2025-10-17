#include "offset_finder.hpp"

auto OffsetFinder::set_default_offsets() -> void {
	// These are the default offsets for the rosetta runtime that matches MD5 hash: c6b7650638eaf4d15bd56b9aae282b11

	offset_loop_copy_func = 0x1289C;                   // Some kind of function that copies n values from param3 to param1 where n is defined by a value in param4.
	offset_svc_call_entry = 0x3c58;                    // The entry point of a function that does a Supervisor Call instruction with the parameter 0x80 (the immediate used by XNU? This is what a quick google search tells me)
	offset_svc_call_ret = offset_svc_call_entry + 0xc; // The return point of the above function

	return;
}

auto OffsetFinder::determine_offsets() -> void {
	// byte patterns in hex for the functions we need to find.
	// I really don't know if it's wise to check for the whole function block, but I'm not really sure how much these can change between versions
	const std::vector<unsigned char> loop_copy_func = {0x62, 0x06, 0x40, 0xf9, 0x63, 0x12, 0x40, 0xb9}; //, 0xe0, 0x05, 0x0f, 0x10, 0x1f, 0x20, 0x03, 0xd5};
	const std::vector<unsigned char> svc_call = {0xb0, 0x18, 0x80, 0xd2, 0x01, 0x10, 0x00, 0xd4, 0xe1, 0x37, 0x9f, 0x9a, 0xc0, 0x03, 0x5f, 0xd6};
	// For svc_call we need to check where this bitpattern starts in the code and also where it ends (we can just add 0xc to the start to get the end)

	// Load rosetta runtime into an ifstream
	std::ifstream file{"/usr/libexec/rosetta/runtime", std::ios::binary};

	// Check if we were successfully able to load the file, if not abort and use default offsets
	if (!file) {
		printf("Problem accessing rosetta runtime to determine offsets automatically.\nFalling back to macOS 15.4.1 defaults (This WILL crash your app if they are not correct!)\n");
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
		printf("Problem reading rosetta runtime to determine offsets automatically.\nFalling back to macOS 15.4.1 defaults (This WILL crash your app if they are not correct!)\n");
		return;
	}

	// Do the search and store the results
	std::vector<std::uint64_t> results;
	for (const auto offset : {loop_copy_func, svc_call}) {
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
		printf("Problem searching rosetta runtime to determine offsets automatically.\nFalling back to macOS 15.4.1 defaults (This WILL crash your app if they are not correct!)\n");
		return;
	}

	// Set the offsets to the results that we've found now that we know they're "correct".
	offset_loop_copy_func = results[0];
	offset_svc_call_entry = results[1];
	offset_svc_call_ret = offset_svc_call_entry + 0xc;

	printf("Found rosetta runtime offsets successfully! noffset_loop_copy_func=%llx offset_svc_call_entry=%llx offset_svc_call_ret=%llx\n", offset_loop_copy_func, offset_svc_call_entry, offset_svc_call_ret);
	return;
}
