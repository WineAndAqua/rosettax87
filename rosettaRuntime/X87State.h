#pragma once

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "Log.h"
#include "X87Float80.h"
#include "X87StackRegister.h"

// Option to convert 64 bit float to 80 bit x87 format to be compatible with
// original rosetta x87 implementation This must be used if you want to
// selectively enable x87 instructions.
// #define X87_CONVERT_TO_FP80

enum X87StatusWordFlag : uint16_t {
	// Exception flags
	kInvalidOperation = 0x0001,    // Invalid Operation Exception
	kDenormalizedOperand = 0x0002, // Denormalized Operand Exception
	kZeroDivide = 0x0004,          // Zero Divide Exception
	kOverflow = 0x0008,            // Overflow Exception–
	kUnderflow = 0x0010,           // Underflow Exception
	kPrecision = 0x0020,           // Precision Exception

	// Status flags
	kStackFault = 0x0040,   // Stack Fault
	kErrorSummary = 0x0080, // Error Summary Status

	// Condition codes
	kConditionCode0 = 0x0100, // Condition Code 0
	kConditionCode1 = 0x0200, // Condition Code 1
	kConditionCode2 = 0x0400, // Condition Code 2
	kConditionCode3 = 0x4000, // Condition Code 3

	// Special flags
	kTopOfStack = 0x3800, // Top of Stack Pointer (bits 11-13)
	kBusy = 0x8000,       // FPU Busy
};

enum class X87TagState {
	kValid = 0,   // 00: Valid non-zero value
	kZero = 1,    // 01: Valid zero value
	kSpecial = 2, // 10: Special value (NaN, Infinity, Denormal)
	kEmpty = 3    // 11: Empty register
};

enum X87ControlWord : uint16_t {
	// Exception Masks (1=masked)
	kInvalidOpMask = 0x0001,
	kDenormalMask = 0x0002,
	kZeroDivideMask = 0x0004,
	kOverflowMask = 0x0008,
	kUnderflowMask = 0x0010,
	kPrecisionMask = 0x0020,

	// Precision Control
	kPrecisionControl = 0x0300,
	kPrecision24Bit = 0x0000, // Single precision
	kPrecision53Bit = 0x0200, // Double precision
	kPrecision64Bit = 0x0300, // Extended precision

	// Rounding Control
	kRoundingControlMask = 0x0C00,
	kRoundToNearest = 0x0000,
	kRoundDown = 0x0400,
	kRoundUp = 0x0800,
	kRoundToZero = 0x0C00,

	// Infinity Control (only on 287)
	kInfinityControl = 0x1000
};

#if defined(X87_CONVERT_TO_FP80)
float inline ConvertX87RegisterToFloat32(X87Float80 x87,  uint16_t *status_flags) {
	uint64_t mantissa = x87.mantissa;
	uint16_t biased_exp = x87.exponent & 0x7FFF;
	uint32_t sign = (x87.exponent & 0x8000) ? 0x80000000 : 0;
	uint32_t bits;

	if (biased_exp == 0 && mantissa == 0) {
		bits = sign;
	} else if (biased_exp == 0x7FFF) {
		// NaN or Infinity
		if ((mantissa & 0x7FFFFFFFFFFFFFFFULL) != 0) {
			if (status_flags)
				*status_flags |= X87StatusWordFlag::kInvalidOperation;
			bits = sign | 0x7FC00000; // Quiet NaN
		} else {
			bits = sign | 0x7F800000; // Infinity
		}
	} else {
		int32_t exp = static_cast<int32_t>(biased_exp) - 16383 + 127;
		uint64_t frac = mantissa & 0x7FFFFFFFFFFFFFFFULL;
		uint32_t significant = static_cast<uint32_t>(frac >> 40);

		uint64_t round_bit = (frac >> 39) & 1;
		uint64_t sticky_bits = frac & 0x7FFFFFFFFF;

		// Underflow or subnormal
		if (exp <= 0) {
			if (status_flags)
				*status_flags |= (X87StatusWordFlag::kUnderflow | X87StatusWordFlag::kPrecision);
			if (exp < -23) {
				bits = sign;
			} else {
				int shift = 1 - exp;
				significant = static_cast<uint32_t>((frac | 0x8000000000000000ULL) >> (40 + shift));
				round_bit = (frac >> (39 + shift)) & 1;
				sticky_bits = frac & ((1ULL << (39 + shift)) - 1);
				exp = 0;
			}
		}

		// Precision exception
		bool inexact = (round_bit || sticky_bits);
		if (inexact && status_flags)
			*status_flags |= X87StatusWordFlag::kPrecision;

		// Round to nearest even and detect overflow
		if (round_bit && (sticky_bits || (significant & 1))) {
			significant++;
			if (significant == 0x800000) {
				significant = 0;
				exp++;
				if (exp >= 255) {
					if (status_flags)
						*status_flags |= X87StatusWordFlag::kOverflow;
					bits = sign | 0x7F800000;
					goto return_float32;
				}
			}
		}

		// Overflow after rounding
		if (exp >= 255) {
			if (status_flags)
				*status_flags |= X87StatusWordFlag::kOverflow;
			bits = sign | 0x7F800000;
		} else {
			bits =
				sign | (static_cast<uint32_t>(exp) << 23) | (significant & 0x7FFFFF);
		}
	}

return_float32:
	union {
		uint32_t u;
		float f;
	} result;
	result.u = bits;
	return result.f;
}

inline X87Float80 ConvertFloat64ToX87Register(double value, uint16_t *status_flags) {
	X87Float80 result;
	union {
		double v;
		uint64_t bits;
	} d{value};
	uint64_t sign = (d.bits >> 63) & 0x1;
	uint64_t exp = (d.bits >> 52) & 0x7FF;
	uint64_t mantissa = d.bits & 0xFFFFFFFFFFFFFULL;

	// Zero
	if (exp == 0 && mantissa == 0) {
		result.exponent = static_cast<uint16_t>(sign << 15);
		result.mantissa = 0;
		return result;
	}

	// NaN or Infinity
	if (exp == 0x7FF) {
		result.exponent = static_cast<uint16_t>((sign << 15) | 0x7FFF);
		if (mantissa == 0) {
			result.mantissa = 0x8000000000000000ULL;
		} else {
			result.mantissa = 0xC000000000000000ULL | (mantissa << 11);
			if (status_flags)
				*status_flags |= X87StatusWordFlag::kInvalidOperation;
		}
		return result;
	}

	// Denormalized double
	if (exp == 0) {
		if (status_flags)
			*status_flags |= X87StatusWordFlag::kDenormalizedOperand;
		int shift = __builtin_clzll(mantissa) - 11;
		mantissa <<= (shift + 1);
		exp = 1 - shift;
	}

	// Normal
	uint16_t x87_exp = static_cast<uint16_t>((exp - 1023) + 16383);
	result.exponent = static_cast<uint16_t>((sign << 15) | x87_exp);
	result.mantissa = (mantissa << 11) | 0x8000000000000000ULL;
	return result;
}

#endif

double inline ConvertX87RegisterToFloat64(X87Float80 x87, uint16_t *status_flags) {
	uint64_t mantissa = x87.mantissa;
	uint16_t biased_exp = x87.exponent & 0x7FFF;
	uint64_t sign = (x87.exponent & 0x8000) ? 0x8000000000000000ULL : 0;
	union {
		uint64_t bits;
		double value;
	} result;

	// Zero
	if (mantissa == 0) {
		return (sign ? -0.0 : 0.0);
	}

	// NaN or Infinity
	if (biased_exp == 0x7FFF) {
		if (mantissa != 0x8000000000000000ULL) {
			if (status_flags)
				*status_flags |= X87StatusWordFlag::kInvalidOperation;
			result.bits = sign | 0x7FF8000000000000ULL;
			return result.value;
		}
		result.bits = sign | 0x7FF0000000000000ULL;
		return result.value;
	}

	int32_t exp = static_cast<int32_t>(biased_exp) - 16383 + 1023;

	// Denormalized / Underflow
	if (exp <= 0) {
		if (status_flags)
			*status_flags |= X87StatusWordFlag::kUnderflow;
		if (exp < -52) {
			return (sign ? -0.0 : 0.0);
		}
		// Denormalize
		mantissa >>= (1 - exp);
		exp = 0;
	}

	// Overflow
	if (exp >= 2047) {
		if (status_flags)
			*status_flags |= X87StatusWordFlag::kOverflow;
		result.bits = sign | 0x7FF0000000000000ULL;
		return result.value;
	}

	// Round to 52 bits
	uint64_t significant = (mantissa >> 11) & 0xFFFFFFFFFFFFFULL;
	uint64_t round_bit = (mantissa >> 10) & 1;
	uint64_t sticky_bits = (mantissa & ((1ULL << 10) - 1)) != 0;

	// Precision exception
	if ((round_bit || sticky_bits) && status_flags) {
		*status_flags |= X87StatusWordFlag::kPrecision;
	}

	// Round to nearest even
	if (round_bit && (sticky_bits || (significant & 1))) {
		significant++;
		if (significant == 0x10000000000000ULL) {
			significant = 0;
			exp++;
			if (exp >= 2047) {
				if (status_flags)
					*status_flags |= X87StatusWordFlag::kOverflow;
				result.bits = sign | 0x7FF0000000000000ULL;
				return result.value;
			}
		}
	}

	result.bits = sign | (static_cast<uint64_t>(exp) << 52) | significant;
	return result.value;
}

#pragma pack(push, 1)
struct X87State {
	uint16_t control_word;
	uint16_t status_word;
	int16_t tag_word;

#if defined(X87_CONVERT_TO_FP80)
	X87Float80 st[8];
#else
	uint8_t padding[2]; // Padding to align to 16 bytes
	X87StackRegister st[8];
#endif

	X87State() : control_word(0x037F), status_word(0x0000), tag_word(0xFFFF) { // All registers marked empty (11)
		// Initialize all registers to zero
		for (int i = 0; i < 8; i++) {
			st[i].ieee754 = 0.0;
		}
	}

	// Get index of top register
	auto top_index() const -> unsigned int {
		return (status_word >> 11) & 7;
	} // Get reference to top register

	// Get index of ST(i) register
	auto get_st_index(unsigned int st_offset) const -> unsigned int {
		return (st_offset + top_index()) & 7;
	}

	// Get value from register at ST(i). Checks tag bits for validity, returns 0.0
	// if empty. Updates status word.
	__attribute__((always_inline)) auto get_st(unsigned int st_offset) -> double {
		const unsigned int reg_idx = get_st_index(st_offset);
		const auto tag = static_cast<X87TagState>((tag_word >> (reg_idx * 2)) & 3);
		if (tag == X87TagState::kEmpty) {
			// FP_X_STK | FP_X_INV
			status_word |= X87StatusWordFlag::kStackFault | X87StatusWordFlag::kInvalidOperation;
			return std::numeric_limits<double>::quiet_NaN();
		}
#if !defined(X87_CONVERT_TO_FP80)
		return st[reg_idx].ieee754;
#else
		return ConvertX87RegisterToFloat64(st[reg_idx], &status_word);
#endif
	}

	auto get_st_const(unsigned int st_offset) const -> std::pair<double, uint16_t> {
		const unsigned int reg_idx = get_st_index(st_offset);
		const X87TagState tag = static_cast<X87TagState>((tag_word >> (reg_idx * 2)) & 3);

		uint16_t new_status_word = status_word & ~(X87StatusWordFlag::kConditionCode1);
		if (tag == X87TagState::kEmpty) {
			// FP_X_STK | FP_X_INV
			//  return nan
			return {std::numeric_limits<double>::quiet_NaN(), new_status_word | X87StatusWordFlag::kStackFault | X87StatusWordFlag::kInvalidOperation};
		}

#if !defined(X87_CONVERT_TO_FP80)
		return {st[reg_idx].ieee754, new_status_word};
#else
		auto value = ConvertX87RegisterToFloat64(st[reg_idx], &new_status_word);
		return {value, new_status_word};
#endif
	}

	auto get_st_const32(unsigned int st_offset) const -> std::pair<float, uint16_t> {
		const unsigned int reg_idx = get_st_index(st_offset);
		const X87TagState tag = static_cast<X87TagState>((tag_word >> (reg_idx * 2)) & 3);

		uint16_t new_status_word =
			status_word & ~(X87StatusWordFlag::kConditionCode1);
		if (tag == X87TagState::kEmpty) {
			// FP_X_STK | FP_X_INV
			//  return nan
			return {std::numeric_limits<float>::quiet_NaN(), new_status_word | X87StatusWordFlag::kStackFault | X87StatusWordFlag::kInvalidOperation};
		}

#if !defined(X87_CONVERT_TO_FP80)
		return {st[reg_idx].ieee754, new_status_word};
#else
		auto value = ConvertX87RegisterToFloat32(st[reg_idx], &new_status_word);
		return {value, new_status_word};
#endif
	}

	__attribute__((always_inline)) auto get_st_tag(unsigned int st_offset) const -> X87TagState {
		const unsigned int reg_idx = get_st_index(st_offset);
		return static_cast<X87TagState>((tag_word >> (reg_idx * 2)) & 3);
	}

	// Push value to FPU stack
	auto push() -> void {
		const int current_top = top_index();
		const int new_top = (current_top - 1) & 7;
		status_word = (status_word & ~X87StatusWordFlag::kTopOfStack) | (new_top << 11);
		// Clear tag bits (set to valid 00) for new register
		tag_word &= ~(3 << (new_top * 2));
	}

	auto pop() -> void {
		const int current_top = top_index();
		// Set tag bits to empty (11) for popped register
		tag_word |= (3 << (current_top * 2));
		st[current_top].ieee754 = 0.0;
		status_word = (status_word & ~X87StatusWordFlag::kTopOfStack) | (((current_top + 1) & 7) << 11);
	}

	__attribute__((always_inline)) auto set_st(unsigned int st_offset, double value) -> void {
		auto st_idx = get_st_index(st_offset);

#if !defined(X87_CONVERT_TO_FP80)
		st[st_idx].ieee754 = value;
#else
		// Convert value to x87 format
		st[st_idx] = ConvertFloat64ToX87Register(value, &status_word);
#endif
		X87TagState tag;
		if (value == 0.0) {
			tag = X87TagState::kZero;
		} else if (std::isnan(value) || std::isinf(value) || std::fpclassify(value) == FP_SUBNORMAL) {
			tag = X87TagState::kSpecial;
		} else {
			tag = X87TagState::kValid;
		}

		// Clear existing tag bits and set new state
		tag_word &= ~(3 << (st_idx * 2));
		tag_word |= (static_cast<int>(tag) << (st_idx * 2));
	}

	__attribute__((always_inline)) auto set_st_fast(unsigned int st_offset, double value) -> void {
		const unsigned int idx = get_st_index(st_offset);

#if !defined(X87_CONVERT_TO_FP80)
		// Direct IEEE-754 store
		st[idx].ieee754 = value;
#else
		// Convert to FP80 format without modifying status_word
		st[idx] = ConvertFloat64ToX87Register(value, nullptr);
#endif

		// Clear both tag bits → 00 (kValid)
		tag_word &= ~(0x3u << (idx * 2));
	}

	// Fast path: bypass tag-checks, assume value valid
	__attribute__((always_inline)) auto get_st_fast(unsigned int st_offset) const -> double {
		// Compute absolute slot index
		const unsigned int idx = get_st_index(st_offset);
#if !defined(X87_CONVERT_TO_FP80)
		// Direct IEEE-754 load
		return st[idx].ieee754;
#else
		// If you still need FP80 support, convert without touching status_word
		return ConvertX87RegisterToFloat64(st[idx], nullptr);
#endif
	}

	auto swap_registers(unsigned int reg_offset1, unsigned int reg_offset2) -> void {
		// Swap register contents
		auto reg_idx1 = get_st_index(reg_offset1);
		auto reg_idx2 = get_st_index(reg_offset2);

		auto temp = st[reg_idx1].ieee754;
		st[reg_idx1].ieee754 = st[reg_idx2].ieee754;
		st[reg_idx2].ieee754 = temp;

		// Get current tags
		const int tag1 = (tag_word >> (reg_idx1 * 2)) & 3;
		const int tag2 = (tag_word >> (reg_idx2 * 2)) & 3;

		// Clear both tags
		tag_word &= ~((3 << (reg_idx1 * 2)) | (3 << (reg_idx2 * 2)));

		// Set swapped tags
		tag_word |= (tag2 << (reg_idx1 * 2)) | (tag1 << (reg_idx2 * 2));
	}

	auto print() const -> void {
		simple_printf("FPU state:\n");
		simple_printf("Control word: %d\n", control_word);
		simple_printf("Status word: %d\n", status_word);
		simple_printf("Tag word: %d\n", tag_word);
		simple_printf("Top index: %d\n", top_index());
		simple_printf("\n");
	}
};
#pragma pack(pop)
#if defined(X87_CONVERT_TO_FP80)
static_assert(sizeof(X87State) == 0x56, "Invalid size for X87State");
#else
static_assert(sizeof(X87State) == 0x48, "Invalid size for X87State");
#endif
static_assert(offsetof(X87State, control_word) == 0, "Invalid offset for X87State::control_word");
static_assert(offsetof(X87State, status_word) == 2, "Invalid offset for X87State::status_word");
static_assert(offsetof(X87State, tag_word) == 4, "Invalid offset for X87State::tag_word");

#if defined(X87_CONVERT_TO_FP80)
static_assert(offsetof(X87State, st) == 6, "Invalid offset for X87State::st0");
#else
static_assert(offsetof(X87State, st) == 0x08, "Invalid offset for X87State::st0");
#endif
