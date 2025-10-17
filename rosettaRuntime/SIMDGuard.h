#pragma once

// most x87 instruction handlers only use d0-d3 registers, so backing all up is
// too much of a penalty. Only sin/cos/log2/exp require a full backup of the
// simd registers. Anything above d7 handled by the compiler. This assumption
// was made by staring at disassembled code.

#include <cstdint>
#define ENABLE_SIMD_GUARD

struct SIMDGuard {
	using SIMDRegister_t = uint8_t[8];

	SIMDGuard() {
#if defined(ENABLE_SIMD_GUARD)
		// Save q0–q3 in pairs into buf
		asm volatile("stp  q0,  q1, [%0, # 0]\n\t"
		             "stp  q2,  q3, [%0, #32]\n\t"
		             : /* no outputs */
		             : "r"(buf)
		             : "memory");
#endif
	}

	~SIMDGuard() {
#if defined(ENABLE_SIMD_GUARD)
		// Restore q0–q7 in reverse order
		asm volatile("ldp  q2,  q3, [%0, #32]\n\t"
		             "ldp  q0,  q1, [%0, # 0]\n\t"
		             :
		             : "r"(buf)
		             : "v0", "v1", "v2", "v3", "memory");
#endif
	}

	alignas(16) uint8_t buf[16][4];
};

struct SIMDGuardFull {
	using SIMDRegister_t = uint8_t[8];

	SIMDGuardFull() {
#if defined(ENABLE_SIMD_GUARD)
		// Save q0–q7 in pairs into buf
		asm volatile("stp  q0,  q1, [%0, # 0]\n\t"
		             "stp  q2,  q3, [%0, #32]\n\t"
		             "stp  q4,  q5, [%0, #64]\n\t"
		             "stp  q6,  q7, [%0, #96]\n\t"
		             : /* no outputs */
		             : "r"(buf)
		             : "memory");
#endif
	}

	~SIMDGuardFull() {
#if defined(ENABLE_SIMD_GUARD)
		// Restore q0–q7 in reverse order
		asm volatile("ldp  q6,  q7, [%0, #96]\n\t"
		             "ldp  q4,  q5, [%0, #64]\n\t"
		             "ldp  q2,  q3, [%0, #32]\n\t"
		             "ldp  q0,  q1, [%0, # 0]\n\t"
		             :
		             : "r"(buf)
		             : "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "memory");
#endif
	}

	alignas(16) uint8_t buf[16][8];
};
