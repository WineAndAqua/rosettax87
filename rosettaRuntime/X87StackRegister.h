#pragma once

#include <cstddef>
#include <cstdint>

#pragma pack(push, 1)
struct X87StackRegister {
#if defined(X87_CONVERT_TO_FP80)
	union {
		struct {
			uint64_t mantissa;
			uint16_t exponent;
		};
		uint8_t bytes[10];
		double ieee754; // 64 bits
	};
#else
	double ieee754; // 64 bits
#endif
};
#pragma pack(pop)

#if defined(X87_CONVERT_TO_FP80)
static_assert(sizeof(X87StackRegister) == 0x0A, "Invalid size for X87StackRegister");
static_assert(offsetof(X87StackRegister, mantissa) == 0, "Invalid offset for X87StackRegister::mantissa");
static_assert(offsetof(X87StackRegister, exponent) == 8, "Invalid offset for X87StackRegister::exponent");
#else
static_assert(sizeof(X87StackRegister) == 0x08, "Invalid size for X87StackRegister");
#endif
static_assert(offsetof(X87StackRegister, ieee754) == 0, "Invalid offset for X87StackRegister::ieee754");
