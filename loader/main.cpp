#include <stdint.h>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <mach-o/dyld.h>
#include <mach-o/dyld_images.h>
#include <mach/mach_vm.h>

#include <map>

#include "macho_loader.hpp"
#include "offset_finder.hpp"

typedef const struct dyld_process_info_base *DyldProcessInfo;

extern "C" DyldProcessInfo _dyld_process_info_create(task_t task, uint64_t timestamp, kern_return_t *kernelError);
extern "C" void _dyld_process_info_for_each_image(DyldProcessInfo info, void (^callback)(uint64_t machHeaderAddress, const uuid_t uuid, const char *path));
extern "C" void _dyld_process_info_release(DyldProcessInfo info);

class MuhDebugger {
private:
	static const unsigned int AARCH64_BREAKPOINT; // just declare here

	pid_t childPid_;
	task_t taskPort_;
	std::map<uint64_t, unsigned int> breakpoints_; // addr -> original instruction

	bool waitForEvent(int *status) {
		if (waitpid(childPid_, status, 0) == -1) {
			perror("waitpid");
			return false;
		}
		if (WIFSTOPPED(*status)) {
			int signal = WSTOPSIG(*status);
			printf("Process stopped signal=%d\n", signal);
			return true;
		}
		return false;
	}

public:
	bool adjustMemoryProtection(uint64_t address, vm_prot_t protection, mach_vm_size_t size) {
		// 4KB page size in rosetta process
		vm_size_t pageSize = 0x1000;
		// align to page boundary
		mach_vm_address_t region = address & ~(pageSize - 1);
		size = ((address + size + pageSize - 1) & ~(pageSize - 1)) - region;

		printf("Adjusting memory protection at 0x%llx - 0x%llx\n", (uint64_t)region, (uint64_t)(region + size));

		kern_return_t kr = mach_vm_protect(taskPort_, region, size, FALSE, protection);
		if (kr != KERN_SUCCESS) {
			printf("Failed to adjust memory protection at 0x%llx - 0x%llx (error 0x%x: %s)\n", (uint64_t)region, (uint64_t)(region + size), kr, mach_error_string(kr));
			return false;
		}
		return true;
	}

	bool attach(pid_t pid) {
		childPid_ = pid;
		printf("Attempting to attach to %d\n", childPid_);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
		if (ptrace(PT_ATTACH, childPid_, 0, 0) < 0) {
#pragma clang diagnostic pop
			perror("ptrace(PT_ATTACH)");
			return false;
		}

		int status;
		waitForEvent(&status);
		printf("Program stopped due to debugger being attached\n");

		if (!continueExecution()) {
			printf("Failed to continue execution\n");
			return false;
		}
		if (task_for_pid(mach_task_self(), childPid_, &taskPort_) != KERN_SUCCESS) {
			printf("Failed to get task port for pid %d\n", childPid_);
			return false;
		}
		printf("Program stopped due to execv into rosetta process.\n");
		printf("Started debugging process %d using port %d\n", childPid_, taskPort_);
		return true;
	}

	bool continueExecution() {
		if (ptrace(PT_CONTINUE, childPid_, (caddr_t)1, 0) < 0) {
			perror("ptrace(PT_CONTINUE)");
			return false;
		}

		printf("continueExecution waiting for event ..\n");

		int status;
		return waitForEvent(&status);
	}

	bool detach() {
		if (ptrace(PT_DETACH, childPid_, (caddr_t)1, 0) < 0) {
			perror("ptrace(PT_DETACH)");
			return false;
		}
		printf("Detached.\n");
		return true;
	}

	struct ModuleInfo {
		uintptr_t address;
		std::string path;
	};

	auto getModuleList() -> std::vector<ModuleInfo> {
		__block std::vector<ModuleInfo> moduleList;
		kern_return_t kr;
		auto process_info = _dyld_process_info_create(taskPort_, 0, &kr);

		if (kr != KERN_SUCCESS) {
			printf("Failed to get dyld process info (error 0x%x: %s)\n", kr, mach_error_string(kr));
			return moduleList;
		}

		_dyld_process_info_for_each_image(process_info, ^(uint64_t address, const uuid_t uuid, const char *path) { moduleList.push_back({address, std::string(path)}); });

		return moduleList;
	}

	auto findRuntime() -> uintptr_t {
		auto moduleList = getModuleList();

		auto runtimeIt = std::find_if(moduleList.begin(), moduleList.end(), [](const ModuleInfo &module) { return module.path == "/usr/libexec/rosetta/runtime"; });
		if (runtimeIt != moduleList.end()) {
			return runtimeIt->address;
		}

		mach_vm_address_t address = 0;
		mach_vm_size_t size;
		vm_region_basic_info_data_64_t info;
		mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
		mach_port_t objectName;

		while (true) {
			if (mach_vm_region(taskPort_, &address, &size, VM_REGION_BASIC_INFO_64, (vm_region_info_t)&info, &count, &objectName) != KERN_SUCCESS) {
				break;
			}

			if (info.protection & (VM_PROT_EXECUTE | VM_PROT_READ)) {
				if (std::find_if(moduleList.begin(), moduleList.end(), [address](const ModuleInfo &module) { return address == module.address; }) == moduleList.end()) {
					uint32_t magicBytes;
					if (readMemory(address, &magicBytes, sizeof(magicBytes)) && magicBytes == MH_MAGIC_64) {
						return address;
					}
				}
			}

			address += size;
		}

		return 0;
	}

	bool setBreakpoint(uint64_t address) {
		// Verify address is in valid range
		if (address >= MACH_VM_MAX_ADDRESS) {
			printf("Invalid address 0x%llx\n", (unsigned long long)address);
			return false;
		}
		unsigned int original;
		mach_vm_size_t readSize;

		// Read the original instruction
		kern_return_t kr = mach_vm_read_overwrite(taskPort_, address, sizeof(unsigned int), (mach_vm_address_t)&original, &readSize);
		if (kr != KERN_SUCCESS) {
			printf("Failed to read memory at 0x%llx (error 0x%x: %s)\n", (unsigned long long)address, kr, mach_error_string(kr));
			return false;
		}

		// First, try to adjust memory protection
		if (!adjustMemoryProtection(address, VM_PROT_READ | VM_PROT_WRITE | VM_PROT_COPY, sizeof(unsigned int))) {
			return false;
		}

		// Write breakpoint instruction
		kr = mach_vm_write(taskPort_, address, (vm_offset_t)&AARCH64_BREAKPOINT, sizeof(unsigned int));
		if (kr != KERN_SUCCESS) {
			printf("Failed to write breakpoint at 0x%llx (error 0x%x: %s)\n", (unsigned long long)address, kr, mach_error_string(kr));
			return false;
		}
		// printf("write success\n");
		if (!adjustMemoryProtection(address, VM_PROT_READ | VM_PROT_EXECUTE, sizeof(unsigned int))) {
			return false;
		}

		// printf("adjustMemoryProtection success\n");
		breakpoints_[address] = original;
		printf("Breakpoint set at address 0x%llx\n", (unsigned long long)address);
		return true;
	}

	bool removeBreakpoint(uint64_t address) {
		auto it = breakpoints_.find(address);
		if (it == breakpoints_.end()) {
			printf("No breakpoint found at address 0x%llx\n", (unsigned long long)address);
			return false;
		}

		// First, try to adjust memory protection
		if (!adjustMemoryProtection(address, VM_PROT_READ | VM_PROT_WRITE, sizeof(unsigned int))) {
			return false;
		}

		// Restore original instruction
		kern_return_t kr = mach_vm_write(taskPort_, address, (vm_offset_t)&it->second, sizeof(unsigned int));
		if (kr != KERN_SUCCESS) {
			printf("Failed to restore original instruction at 0x%llx (error 0x%x: %s)\n", (unsigned long long)address, kr, mach_error_string(kr));
			return false;
		}
		if (!adjustMemoryProtection(address, VM_PROT_READ | VM_PROT_EXECUTE, sizeof(unsigned int))) {
			return false;
		}
		breakpoints_.erase(it);
		printf("Breakpoint removed from address 0x%llx\n", (unsigned long long)address);
		return true;
	}

	enum Register {
		X0, X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15,
		X16, X17, X18, X19, X20, X21, X22, X23, X24, X25, X26, X27, X28,
		FP, LR, SP, PC, CPSR
	};

	uint64_t readRegister(Register reg) {
		thread_act_port_array_t threadList;
		mach_msg_type_number_t threadCount;

		kern_return_t kr = task_threads(taskPort_, &threadList, &threadCount);
		if (kr != KERN_SUCCESS) {
			printf("Failed to get threads (error 0x%x: %s)\n", kr, mach_error_string(kr));
			return 0;
		}

		arm_thread_state64_t state;
		mach_msg_type_number_t count = ARM_THREAD_STATE64_COUNT;
		kr = thread_get_state(threadList[0], ARM_THREAD_STATE64, (thread_state_t)&state, &count);

		if (kr != KERN_SUCCESS) {
			printf("Failed to get thread state (error 0x%x: %s)\n", kr, mach_error_string(kr));
			return 0;
		}

		uint64_t value = 0;
		if (reg >= X0 && reg <= X28) {
			value = state.__x[reg];
		} else {
			switch (reg) {
			case FP:
				value = state.__fp;
				break;
			case LR:
				value = state.__lr;
				break;
			case SP:
				value = state.__sp;
				break;
			case PC:
				value = state.__pc;
				break;
			case CPSR:
				value = state.__cpsr;
				break;
			default: {
				printf("Invalid register\n");
				return 0;
			}
			}
		}

		// Cleanup
		for (unsigned int i = 0; i < threadCount; i++) {
			mach_port_deallocate(mach_task_self(), threadList[i]);
		}
		vm_deallocate(mach_task_self(), (vm_address_t)threadList, sizeof(thread_t) * threadCount);

		return value;
	}

	bool setRegister(Register reg, uint64_t value) {
		thread_act_port_array_t threadList;
		mach_msg_type_number_t threadCount;

		kern_return_t kr = task_threads(taskPort_, &threadList, &threadCount);
		if (kr != KERN_SUCCESS) {
			printf("Failed to get threads (error 0x%x: %s)\n", kr, mach_error_string(kr));
			return false;
		}

		arm_thread_state64_t state;
		mach_msg_type_number_t count = ARM_THREAD_STATE64_COUNT;
		kr = thread_get_state(threadList[0], ARM_THREAD_STATE64, (thread_state_t)&state, &count);

		if (kr != KERN_SUCCESS) {
			printf("Failed to get thread state (error 0x%x: %s)\n", kr, mach_error_string(kr));
			return false;
		}

		if (reg >= X0 && reg <= X28) {
			state.__x[reg] = value;
		} else {
			switch (reg) {
			case FP:
				state.__fp = value;
				break;
			case LR:
				state.__lr = value;
				break;
			case SP:
				state.__sp = value;
				break;
			case PC:
				state.__pc = value;
				break;
			case CPSR:
				state.__cpsr = value;
				break;
			default: {
				printf("Invalid register\n");
				return false;
			}
			}
		}

		kr = thread_set_state(threadList[0], ARM_THREAD_STATE64, (thread_state_t)&state, ARM_THREAD_STATE64_COUNT);
		if (kr != KERN_SUCCESS) {
			printf("Failed to set thread state (error 0x%x: %s)\n", kr, mach_error_string(kr));
			return false;
		}

		// Cleanup
		for (unsigned int i = 0; i < threadCount; i++) {
			mach_port_deallocate(mach_task_self(), threadList[i]);
		}
		vm_deallocate(mach_task_self(), (vm_address_t)threadList, sizeof(thread_t) * threadCount);

		return true;
	}

	bool readMemory(uint64_t address, void *buffer, size_t size) {
		mach_vm_size_t readSize;

		kern_return_t kr = mach_vm_read_overwrite(taskPort_, address, size, (mach_vm_address_t)buffer, &readSize);

		if (kr != KERN_SUCCESS) {
			printf("Failed to read memory at 0x%llx (error 0x%x: %s)\n", (unsigned long long)address, kr, mach_error_string(kr));
			return false;
		}

		return readSize == size;
	}

	bool writeMemory(uint64_t address, const void *buffer, size_t size) {
		kern_return_t kr = mach_vm_write(taskPort_, address, (vm_offset_t)buffer, size);

		if (kr != KERN_SUCCESS) {
			printf("Failed to write memory at 0x%llx (error 0x%x: %s)\n", (unsigned long long)address, kr, mach_error_string(kr));
			return false;
		}

		return true;
	}

	uint64_t allocateMemory(size_t size) {
		mach_vm_address_t address = 0; // Let system choose the address

		kern_return_t kr = mach_vm_allocate(taskPort_, &address, size, VM_FLAGS_ANYWHERE);

		if (kr != KERN_SUCCESS) {
			printf("Failed to allocate memory (error 0x%x: %s)\n", kr, mach_error_string(kr));
			return 0;
		}

		// Set memory protection to RWX
		if (!adjustMemoryProtection(address, VM_PROT_READ | VM_PROT_WRITE, size)) {
			// If protection fails, deallocate the memory
			mach_vm_deallocate(taskPort_, address, size);
			return 0;
		}

		printf("Allocated %zu bytes at 0x%llx\n", size, (unsigned long long)address);
		return address;
	}

	bool copyThreadState(arm_thread_state64_t &state) {
		thread_act_port_array_t threadList;
		mach_msg_type_number_t threadCount;

		kern_return_t kr = task_threads(taskPort_, &threadList, &threadCount);
		if (kr != KERN_SUCCESS) {
			printf("Failed to get threads (error 0x%x: %s)\n", kr, mach_error_string(kr));
			return false;
		}

		mach_msg_type_number_t count = ARM_THREAD_STATE64_COUNT;
		kr = thread_get_state(threadList[0], ARM_THREAD_STATE64, (thread_state_t)&state, &count);

		// Cleanup
		for (unsigned int i = 0; i < threadCount; i++) {
			mach_port_deallocate(mach_task_self(), threadList[i]);
		}
		vm_deallocate(mach_task_self(), (vm_address_t)threadList, sizeof(thread_t) * threadCount);

		if (kr != KERN_SUCCESS) {
			printf("Failed to get thread state (error 0x%x: %s)\n", kr, mach_error_string(kr));
			return false;
		}

		return true;
	}

	bool restoreThreadState(const arm_thread_state64_t &state) {
		thread_act_port_array_t threadList;
		mach_msg_type_number_t threadCount;

		kern_return_t kr = task_threads(taskPort_, &threadList, &threadCount);
		if (kr != KERN_SUCCESS) {
			printf("Failed to get threads (error 0x%x: %s)\n", kr, mach_error_string(kr));
			return false;
		}

		kr = thread_set_state(threadList[0], ARM_THREAD_STATE64, (thread_state_t)&state, ARM_THREAD_STATE64_COUNT);

		// Cleanup
		for (unsigned int i = 0; i < threadCount; i++) {
			mach_port_deallocate(mach_task_self(), threadList[i]);
		}
		vm_deallocate(mach_task_self(), (vm_address_t)threadList, sizeof(thread_t) * threadCount);

		if (kr != KERN_SUCCESS) {
			printf("Failed to set thread state (error 0x%x: %s)\n", kr, mach_error_string(kr));
			return false;
		}

		return true;
	}

	~MuhDebugger() {
		if (taskPort_ != MACH_PORT_NULL) {
			mach_port_deallocate(mach_task_self(), taskPort_);
		}
	}
};

// Define the static constant outside the class
const unsigned int MuhDebugger::AARCH64_BREAKPOINT = 0xD4200000;

struct Exports {
	uint64_t version; // 0x16A0000000000
	uint64_t x87Exports;
	uint64_t x87ExportCount;
	uint64_t runtimeExports;
	uint64_t runtimeExportCount;
};

struct Export {
	uint64_t address;
	uint64_t name;
};

int main(int argc, char *argv[]) {
	if (argc < 2) {
		printf("call with program\n");
		return 1;
	}

	printf("Launching debugger.\n");

	// Fork and execute new instance
	pid_t child = fork();

	// the debugger will be this process debugging its child
	if (child == 0) {
		// the fresh child waiting to be debugged
		if (ptrace(PT_TRACE_ME, 0, nullptr, 0) == -1) {
			perror("child: ptrace(PT_TRACE_ME)");
			return 1;
		}
		printf("child: launching into program\n");
		execv(argv[1], &argv[1]);
		return 1;
	}

	MuhDebugger dbg;
	if (!dbg.attach(child)) {
		printf("Failed to attach to process\n");
		return 1;
	}
	printf("Attached successfully\n");

	// Set up offsets dynamically
	OffsetFinder offsetFinder;
	// Set default offsets temporarily (or just in case we need to fall back)
	offsetFinder.setDefaultOffsets();
	// Search the rosetta runtime binary for offsets.
	offsetFinder.determineOffsets();

	auto moduleList = dbg.getModuleList();

	for (const auto &module : moduleList) {
		printf("address %lx, name %s\n", module.address, module.path.c_str());
	}

	const auto runtimeBase = dbg.findRuntime();

	printf("Rosetta runtime base: 0x%lx\n", runtimeBase);

	if (runtimeBase == 0) {
		printf("Failed to find Rosetta runtime\n");
		return 1;
	}

	dbg.setBreakpoint(runtimeBase + offsetFinder.offsetExportsFetch_);
	dbg.continueExecution();
	dbg.removeBreakpoint(runtimeBase + offsetFinder.offsetExportsFetch_);

	auto rosettaRuntimeExportsAddress = dbg.readRegister(MuhDebugger::Register::X19);
	printf("Rosetta runtime exports: 0x%llx\n", rosettaRuntimeExportsAddress);

	Exports exports;
	dbg.readMemory(rosettaRuntimeExportsAddress, &exports, sizeof(exports));

	printf("Rosetta version: %llx\n", exports.version);

	char path[PATH_MAX];
	uint32_t path_size = sizeof(path);
	if (_NSGetExecutablePath(path, &path_size) != 0) {
		printf("Failed to get executable path\n");
		return 1;
	}

	// get the directory of the current executable
	std::filesystem::path executablePath(path);
	std::filesystem::path executableDir = executablePath.parent_path();

	MachoLoader machoLoader;

	if (!machoLoader.open(executableDir / "libRuntimeRosettax87")) {
		printf("Failed to open Mach-O file\n");
		return 1;
	}

	// we need to call mmap to allocate the memory for our macho

	uint64_t machoBase = 0; // dbg.allocateMemory(macho_loader.image_size());

	// first we store the original state of the thread
	arm_thread_state64_t backupThreadState;
	dbg.copyThreadState(backupThreadState);

	// now we prepare the registers for the mmap call
	arm_thread_state64_t mmapThreadState;
	memcpy(&mmapThreadState, &backupThreadState, sizeof(arm_thread_state64_t));

	mmapThreadState.__x[0] = 0LL;                                     // addr
	mmapThreadState.__x[1] = machoLoader.imageSize();                 // size
	mmapThreadState.__x[2] = VM_PROT_READ | VM_PROT_WRITE;            // prot
	mmapThreadState.__x[3] = MAP_ANON | MAP_TRANSLATED_ALLOW_EXECUTE; // flags
	mmapThreadState.__x[4] = -1;                                      // fd
	mmapThreadState.__x[5] = 0;                                       // offset
	mmapThreadState.__pc = runtimeBase + offsetFinder.offsetSvcCallEntry_;

	dbg.restoreThreadState(mmapThreadState);

	// setup a breakpoint after mmap syscall
	dbg.setBreakpoint(runtimeBase + offsetFinder.offsetSvcCallRet_);
	dbg.continueExecution();
	dbg.removeBreakpoint(runtimeBase + offsetFinder.offsetSvcCallRet_);

	machoBase = dbg.readRegister(MuhDebugger::Register::X0);

	printf("Allocated memory at 0x%llx\n", machoBase);

	dbg.restoreThreadState(backupThreadState);

	machoLoader.forEachSegment([&](segment_command_64 *segm) {
		auto dest = machoBase + segm->vmaddr;
		auto size = segm->vmsize;
		auto src = machoLoader.buffer_.data() + segm->fileoff;

		printf("Copying segment %s from 0x%llx to 0x%llx (%zx bytes)\n", segm->segname, (unsigned long long)segm->fileoff, (unsigned long long)dest, (unsigned long)size);

		dbg.writeMemory(dest, src, size);

		dbg.adjustMemoryProtection(dest, segm->initprot, segm->vmsize);
	});

	// fix up Exports segment of mapped macho
	uint64_t machoExportsAddress = machoBase + machoLoader.getSection("__DATA", "exports")->addr;
	Exports machoExports;

	dbg.readMemory(machoExportsAddress, &machoExports, sizeof(machoExports));
	machoExports.x87Exports += machoBase;
	machoExports.runtimeExports += machoBase;

	std::vector<Export> x87Exports(machoExports.x87ExportCount);
	std::vector<Export> runtimeExports(machoExports.runtimeExportCount);

	dbg.readMemory(machoExports.x87Exports, x87Exports.data(), x87Exports.size() * sizeof(Export));
	dbg.readMemory(machoExports.runtimeExports, runtimeExports.data(), runtimeExports.size() * sizeof(Export));

	for (auto &exp : x87Exports) {
		exp.address += machoBase;
		exp.name += machoBase;
	}

	for (auto &exp : runtimeExports) {
		exp.address += machoBase;
		exp.name += machoBase;
	}

	dbg.writeMemory(machoExports.x87Exports, x87Exports.data(), x87Exports.size() * sizeof(Export));
	dbg.writeMemory(machoExports.runtimeExports, runtimeExports.data(), runtimeExports.size() * sizeof(Export));

	printf("machoExports_address: 0x%llx\n", machoExportsAddress);
	printf("machoExports.x87Exports: 0x%llx\n", machoExports.x87Exports);
	printf("machoExports.runtimeExports: 0x%llx\n", machoExports.runtimeExports);

	dbg.writeMemory(machoExportsAddress, &machoExports, sizeof(machoExports));

	// look up imports section of mapped macho
	auto machoImportsAddress = machoBase + machoLoader.getSection("__DATA", "imports")->addr;
	printf("machoImportsAddress: 0x%llx\n", machoImportsAddress);

	// read the exports from X19 register and copy them to the imports section of the mapped macho
	auto libRosettaRuntimeExportsAddress = dbg.readRegister(MuhDebugger::Register::X19);
	printf("libRosettaRuntimeExportsAddress: 0x%llx\n", libRosettaRuntimeExportsAddress);

	Exports libRosettaRuntimeExports;
	dbg.readMemory(libRosettaRuntimeExportsAddress, &libRosettaRuntimeExports, sizeof(libRosettaRuntimeExports));

	printf("libRosettaRuntimeExports.version = 0x%llx\n", libRosettaRuntimeExports.version);
	printf("libRosettaRuntimeExports.x87Exports = 0x%llx\n", libRosettaRuntimeExports.x87Exports);
	printf("libRosettaRuntimeExports.x87Export_count = 0x%llx\n", libRosettaRuntimeExports.x87ExportCount);
	printf("libRosettaRuntimeExports.runtimeExports = 0x%llx\n", libRosettaRuntimeExports.runtimeExports);
	printf("libRosettaRuntimeExports.runtimeExportCount = 0x%llx\n", libRosettaRuntimeExports.runtimeExportCount);

	dbg.writeMemory(machoImportsAddress, &libRosettaRuntimeExports, sizeof(libRosettaRuntimeExports));

	// replace the exports in X19 register with the address of the mapped macho
	dbg.setRegister(MuhDebugger::Register::X19, machoExportsAddress);

	dbg.detach();

	// block until the child exits
	int status;
	waitpid(child, &status, 0);

	return 0;
}
