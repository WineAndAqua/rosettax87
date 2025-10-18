#include "X87.h"
#include "Export.h"
#include "Log.h"
#include "SIMDGuard.h"
#include "X87State.h"

#include <cstring>

#define X87_F2XM1
#define X87_FABS
#define X87_FADD_ST
#define X87_FADD_F32
#define X87_FADD_F64
#define X87_FBLD
#define X87_FBSTP
#define X87_FCHS
#define X87_FCMOV
#define X87_FCOM_ST
#define X87_FCOM_F32
#define X87_FCOM_F64
#define X87_FCOMI
#define X87_FCOS
#define X87_FDECSTP
#define X87_FDIV_ST
#define X87_FDIV_F32
#define X87_FDIV_F64
#define X87_FDIVR_ST
#define X87_FDIVR_F32
#define X87_FDIVR_F64
#define X87_FFREE
#define X87_FIADD
#define X87_FICOM
#define X87_FIDIV
#define X87_FIDIVR
#define X87_FILD
#define X87_FIMUL
#define X87_FINCSTP
#define X87_FIST_I16
#define X87_FIST_I32
#define X87_FIST_I64
#define X87_FISTT_I16
#define X87_FISTT_I32
#define X87_FISTT_I64
#define X87_FISUB
#define X87_FISUBR
#define X87_FLD_STI
#define X87_FLD_CONSTANT
#define X87_FLD_FP32
#define X87_FLD_FP64
#define X87_FLD_FP80
#define X87_FMUL_ST
#define X87_FMUL_F32
#define X87_FMUL_F64
#define X87_FPATAN
#define X87_FPREM
#define X87_FPREM1
#define X87_FPTAN
#define X87_FRNDINT
#define X87_FSCALE
#define X87_FSIN
#define X87_FSINCOS
#define X87_FSQRT
#define X87_FST_STI
#define X87_FST_FP32
#define X87_FST_FP64
#define X87_FST_FP80
#define X87_FSUB_ST
#define X87_FSUB_F32
#define X87_FSUB_F64
#define X87_FSUBR_ST
#define X87_FSUBR_F32
#define X87_FSUBR_F64
#define X87_FUCOM
#define X87_FUCOMI
#define X87_FXAM
#define X87_FXCH
#define X87_FXTRACT
#define X87_FYL2X
#define X87_FYL2XP1

#define X87_TRAMPOLINE(NAME, REGISTER)                                         \
	void __attribute__((naked, used)) NAME() {                             \
		asm volatile("adrp " #REGISTER ", _orig_" #NAME "@PAGE\n"      \
		             "ldr " #REGISTER ", [" #REGISTER ", _orig_" #NAME \
		             "@PAGEOFF]\n"                                     \
		             "br " #REGISTER);                                 \
	}

#define X87_TRAMPOLINE_ARGS(RETURN, NAME, ARGS, REGISTER)                      \
	RETURN __attribute__((naked, used)) NAME ARGS {                        \
		asm volatile("adrp " #REGISTER ", _orig_" #NAME "@PAGE\n"      \
		             "ldr " #REGISTER ", [" #REGISTER ", _orig_" #NAME \
		             "@PAGEOFF]\n"                                     \
		             "br " #REGISTER);                                 \
	}

void *init_library(SymbolList const *a1, unsigned long long a2, ThreadContextOffsets const *a3) {
	SIMDGuardFull simdGuard;
	exportsInit();

	simplePrintf("RosettaRuntimex87 built %s\n", __DATE__ " " __TIME__);

	return orig_init_library(a1, a2, a3);
}

X87_TRAMPOLINE(register_runtime_routine_offsets, x9)
X87_TRAMPOLINE(translator_use_t8027_codegen, x9)
X87_TRAMPOLINE(translator_reset, x9)
X87_TRAMPOLINE(ir_create_bad_access, x9)
X87_TRAMPOLINE(ir_create, x9)
X87_TRAMPOLINE(module_free, x9)
X87_TRAMPOLINE(module_get_size, x9)
X87_TRAMPOLINE(module_is_bad_access, x9)
X87_TRAMPOLINE(module_print, x9)
X87_TRAMPOLINE(translator_translate, x9)
X87_TRAMPOLINE(translator_free, x9)
X87_TRAMPOLINE(translator_get_data, x9)
X87_TRAMPOLINE(translator_get_size, x9)
X87_TRAMPOLINE(translator_get_branch_slots_offset, x9)
X87_TRAMPOLINE(translator_get_branch_slots_count, x9)
X87_TRAMPOLINE(translator_get_branch_entries, x9)
X87_TRAMPOLINE(translator_get_instruction_offsets, x9)
X87_TRAMPOLINE(translator_apply_fixups, x9)

#if !defined(X87_CONVERT_TO_FP80)
void x87_init(X87State *a1) {
	SIMDGuardFull simdGuard;
	LOG(1, "x87_init\n", 9);
	*a1 = X87State();
}
#else
X87_TRAMPOLINE_ARGS(void, x87_init, (X87State * a1), x9);
#endif

X87_TRAMPOLINE(x87_state_from_x86_float_state, x9);
X87_TRAMPOLINE(x87_state_to_x86_float_state, x9);
X87_TRAMPOLINE(x87_pop_register_stack, x9);

#if defined(X87_F2XM1)
void x87_f2xm1(X87State *state) {
	SIMDGuard simdGuard;

	LOG(1, "x87_f2xm1\n", 10);
	// Get value from ST(0)
	auto x = state->getStFast(0);

	// // Check range [-1.0, +1.0]
	if (x < -1.0f || x > 1.0f) {
		// Set to NaN for undefined result
		state->setStFast(0, 0);
		return;
	}

	// Calculate 2^x - 1 using mmath::exp2
	auto result = exp2(x) - 1.0f;

	// Store result back in ST(0)
	state->setStFast(0, result);
}
#else
X87_TRAMPOLINE_ARGS(void, x87_f2xm1, (X87State * state), x9);
#endif

// Clears the sign bit of ST(0) to create the absolute value of the operand. The
// following table shows the results obtained when creating the absolute value
// of various classes of numbers. C1 Set to 0.
#if defined(X87_FABS)
void x87_fabs(X87State *a1) {
	SIMDGuard simdGuard;

	LOG(1, "x87_fabs\n", 10);

	// Clear condition code 1 and exception flags
	a1->statusWord &= ~X87StatusWordFlag::kConditionCode1;

	// Get value in ST(0)
	auto value = a1->getStFast(0);

	// Set value in ST(0) to its absolute value
	a1->setStFast(0, std::abs(value));
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fabs, (X87State * a1), x9);
#endif

#if defined(X87_FADD_ST)
void x87_fadd_ST(X87State *a1, unsigned int st_offset_1, unsigned int st_offset_2, bool pop_stack) {
	SIMDGuard simdGuard;

	LOG(1, "x87_fadd_ST\n", 13);
	// Clear condition code 1 and exception flags
	a1->statusWord &= ~X87StatusWordFlag::kConditionCode1;

	// Get register indices and values
	const auto val1 = a1->getStFast(st_offset_1);
	const auto val2 = a1->getStFast(st_offset_2);

	// Perform addition and store result in ST(idx1)
	a1->setStFast(st_offset_1, val1 + val2);

	if (pop_stack) {
		a1->pop();
	}
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fadd_ST, (X87State * a1, unsigned int st_offset_1, unsigned int st_offset_2, bool pop_stack), x9);
#endif

#if defined(X87_FADD_F32)
void x87_fadd_f32(X87State *a1, unsigned int fp32) {
	SIMDGuard simdGuard;

	LOG(1, "x87_fadd_f32\n", 14);

	a1->statusWord &= ~X87StatusWordFlag::kConditionCode1;

	auto value = std::bit_cast<float>(fp32);
	auto st0 = a1->getStFast(0);

	a1->setStFast(0, st0 + value);
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fadd_f32, (X87State * a1, unsigned int fp32), x9);
#endif

#if defined(X87_FADD_F64)
void x87_fadd_f64(X87State *a1, unsigned long long a2) {
	SIMDGuard simdGuard;

	LOG(1, "x87_fadd_f64\n", 14);

	a1->statusWord &= ~X87StatusWordFlag::kConditionCode1;

	auto value = std::bit_cast<double>(a2);
	auto st0 = a1->getStFast(0);

	a1->setStFast(0, st0 + value);
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fadd_f64, (X87State * a1, unsigned long long a2), x9);
#endif

double BCD2Double(uint8_t bcd[10]) {
	uint64_t tmp = 0;
	uint64_t mult = 1;
	uint8_t piece;

	for (int i = 0; i < 9; ++i) {
		piece = bcd[i];
		tmp += mult * (piece & 0x0F);
		mult *= 10;
		tmp += mult * ((piece >> 4) & 0x0F);
		mult *= 10;
	}

	piece = bcd[9];
	tmp += mult * (piece & 0x0F);

	double value = static_cast<double>(tmp);

	if (piece & 0x80) {
		value = -value;
	}

	return value;
}

#if defined(X87_FBLD)
void x87_fbld(X87State *a1, unsigned long long a2, unsigned long long a3) {
	SIMDGuard simdGuard;
	LOG(1, "x87_fbld\n", 10);

	// set C1 to 0
	a1->statusWord &= ~X87StatusWordFlag::kConditionCode1;

	uint8_t bcd[10];
	memcpy(bcd, &a2, 8);     // Copy 8 bytes from a2
	memcpy(bcd + 8, &a3, 2); // Copy 2 bytes from a3

	auto value = BCD2Double(bcd);

	// Add space on the stack and push the converted BCD
	a1->push();
	a1->setSt(0, value);
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fbld, (X87State * a1, unsigned long long a2, unsigned long long a3), x9);
#endif

#if defined(X87_FBSTP)
uint128_t x87_fbstp(X87State *a1) {
	LOG(1, "x87_fbstp\n", 11);

	auto st0 = a1->getSt(0);
	a1->pop();

	// convert double to BCD
	uint8_t bcd[10] = {0}; // Initialize all bytes to 0

	// Handle sign
	bool is_negative = signbit(st0);

	// Handle special cases
	if (isnan(st0) || isinf(st0)) {
		// Set to indefinite BCD value
		memset(bcd, 0, 10);
		if (is_negative) {
			bcd[9] = 0x80; // Set sign bit if negative
		}
	} else {
		// Get absolute value
		double abs_value = fabs(st0);

		// Truncate to integer
		abs_value = trunc(abs_value);

		// Check if value is too large for BCD format (more than 18 decimal digits)
		if (abs_value > 999999999999999999.0) {
			// Handle overflow - set to maximum BCD value
			memset(bcd, 0x99, 9); // Set first 9 bytes to 0x99 (all digits = 9)
			bcd[9] = 0x09;        // Set last digit to 9
			if (is_negative) {
				bcd[9] |= 0x80; // Set sign bit if negative
			}
		} else {
			// Convert to BCD representation
			uint64_t integer_part = static_cast<uint64_t>(abs_value);

			// Process each byte (2 decimal digits per byte)
			for (int i = 0; i < 9; i++) {
				uint8_t digit1 = integer_part % 10;
				integer_part /= 10;
				uint8_t digit2 = integer_part % 10;
				integer_part /= 10;

				bcd[i] = digit1 | (digit2 << 4);
			}

			// Handle the 10th byte (contains 1 digit and sign)
			bcd[9] = integer_part % 10;
			if (is_negative) {
				bcd[9] |= 0x80; // Set sign bit if negative
			}
		}
	}

	return {
		.low = reinterpret_cast<uint64_t *>(bcd)[0],
		.high = reinterpret_cast<uint64_t *>(bcd)[1],
	};
}
#else
X87_TRAMPOLINE_ARGS(uint128_t, x87_fbstp, (X87State * a1), x9);
#endif

#if defined(X87_FCHS)
void x87_fchs(X87State *a1) {
	SIMDGuard simdGuard;

	LOG(1, "x87_fchs\n", 10);
	// set C1 to 0
	a1->statusWord &= ~X87StatusWordFlag::kConditionCode1;

	// Negate value in ST(0)
	a1->setStFast(0, -a1->getStFast(0));
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fchs, (X87State * a1), x9);
#endif

#if defined(X87_FCMOV)
void x87_fcmov(X87State *state, unsigned int condition, unsigned int st_offset) {
	SIMDGuard simdGuard;

	LOG(1, "x87_fcmov\n", 11);

	// clear precision flag
	state->statusWord &= ~X87StatusWordFlag::kConditionCode1;

	double value;

	auto stTagWord = state->getStTag(st_offset);
	if (stTagWord != X87TagState::kEmpty) {
		if (condition == 0) {
			return;
		}

		value = state->getSt(st_offset);
	} else {
		state->statusWord |= 0x41; // Set invalid operation
		value = 0.0f;
	}

	state->setSt(0, value); // Perform the actual register move
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fcmov, (X87State * state, unsigned int condition, unsigned int st_offset), x9);
#endif

#if defined(X87_FCOM_ST)
void x87_fcom_ST(X87State *a1, unsigned int st_offset, unsigned int number_of_pops) {
	SIMDGuard simdGuard;

	LOG(1, "x87_fcom_ST\n", 13);

	// Get values to compare
	auto st0 = a1->getSt(0);
	auto src = a1->getSt(st_offset);

	// Clear condition code bits C0, C2, C3 (bits 8, 9, 14)
	a1->statusWord &= ~(kConditionCode0 | kConditionCode2 | kConditionCode3);

	// Set condition codes based on comparison
	if (st0 > src) {
		// Leave C0=C2=C3=0
	} else if (st0 < src) {
		a1->statusWord |= kConditionCode0; // Set C0=1
	} else {                                // st0 == sti
		a1->statusWord |= kConditionCode3; // Set C3=1
	}

	if ((a1->controlWord & kInvalidOpMask) == kInvalidOpMask) {
		if (isnan(st0) || isnan(src)) {
			a1->statusWord |= kConditionCode0 | kConditionCode2 | kConditionCode3; // Set C0=C2=C3=1
		}
	}

	// Handle pops if requested
	for (unsigned int i = 0; i < number_of_pops; i++) {
		a1->pop();
	}
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fcom_ST, (X87State * a1, unsigned int st_offset, unsigned int number_of_pops), x9);
#endif

#if defined(X87_FCOM_F32)
void x87_fcom_f32(X87State *a1, unsigned int fp32, bool pop) {
	SIMDGuard simdGuard;

	LOG(1, "x87_fcom_f32\n", 14);
	auto st0 = a1->getSt(0);
	auto src = std::bit_cast<float>(fp32);

	a1->statusWord &= ~(kConditionCode0 | kConditionCode1 | kConditionCode2 | kConditionCode3);

	if (st0 > src) {
		// Leave C0=C2=C3=0
	} else if (st0 < src) {
		a1->statusWord |= kConditionCode0; // Set C0=1
	} else {                                // st0 == value
		a1->statusWord |= kConditionCode3; // Set C3=1
	}

	if ((a1->controlWord & kInvalidOpMask) == kInvalidOpMask) {
		if (isnan(st0) || isnan(src)) {
			a1->statusWord |= kConditionCode0 | kConditionCode2 | kConditionCode3; // Set C0=C2=C3=1
		}
	}

	if (pop) {
		a1->pop();
	}
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fcom_f32, (X87State * a1, unsigned int fp32, bool pop), x9);
#endif

#if defined(X87_FCOM_F64)
void x87_fcom_f64(X87State *a1, unsigned long long fp64, bool pop) {
	SIMDGuard simdGuard;

	LOG(1, "x87_fcom_f64\n", 14);
	auto st0 = a1->getSt(0);
	auto src = std::bit_cast<double>(fp64);

	a1->statusWord &= ~(kConditionCode0 | kConditionCode2 | kConditionCode3);

	if (st0 > src) {
		// Leave C0=C2=C3=0
	} else if (st0 < src) {
		a1->statusWord |= kConditionCode0; // Set C0=1
	} else {                                // st0 == value
		a1->statusWord |= kConditionCode3; // Set C3=1
	}

	if ((a1->controlWord & kInvalidOpMask) == kInvalidOpMask) {
		if (isnan(st0) || isnan(src)) {
			a1->statusWord |= kConditionCode0 | kConditionCode2 | kConditionCode3; // Set C0=C2=C3=1
		}
	}

	if (pop) {
		a1->pop();
	}
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fcom_f64, (X87State * a1, unsigned long long fp64, bool pop), x9);
#endif

#if defined(X87_FCOMI)
uint32_t x87_fcomi(X87State *state, unsigned int st_offset, bool pop) {
	SIMDGuard simdGuard;

	LOG(1, "x87_fcomi\n", 11);
	state->statusWord &= ~(kConditionCode0);

	auto st0_val = state->getSt(0);
	auto sti_val = state->getSt(st_offset);

	uint32_t flags = 0;
	/*
	Filters: fcomi
	Randomness seeded to: 3528984885
	x87_fcomi_less
	x87_fcomi result: 0x000000000000000
	x87_fcomi_greater
	x87_fcomi result: 0x000000020000000
	x87_fcomi_equal
	x87_fcomi result: 0x000000060000000
	*/

	if (st0_val < sti_val) {
		flags = 0x000000000000000;
	} else if (st0_val > sti_val) {
		flags = 0x000000020000000;
	} else {
		flags = 0x000000060000000;
	}

	if (pop) {
		state->pop();
	}

	return flags;
}
#else
X87_TRAMPOLINE_ARGS(uint32_t, x87_fcomi, (X87State * state, unsigned int st_offset, bool pop), x9);
#endif

#if defined(X87_FCOS)
void x87_fcos(X87State *a1) {
	SIMDGuardFull simdGuard;

	LOG(1, "x87_fcos\n", 10);
	a1->statusWord &= ~(kConditionCode1 | kConditionCode2);
	// Get ST(0)
	auto value = a1->getStFast(0);

	// Calculate cosine
	auto result = cos(value);

	// Store result back in ST(0)
	a1->setStFast(0, result);
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fcos, (X87State * a1), x9);
#endif

#if defined(X87_FDECSTP)
void x87_fdecstp(X87State *a1) {
	LOG(1, "x87_fdecstp\n", 13);

	uint16_t current_top = (a1->statusWord & X87StatusWordFlag::kTopOfStack) >> 11;

	// Decrement the top of stack pointer (wrapping from 0 to 7)
	uint16_t new_top = (current_top - 1) & 7;

	// Clear C1
	a1->statusWord &= ~X87StatusWordFlag::kConditionCode1;
	// Clear the top of stack bits and set the new value
	a1->statusWord = (a1->statusWord & ~X87StatusWordFlag::kTopOfStack) | (new_top << 11);
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fdecstp, (X87State * a1), x9);
#endif

#if defined(X87_FDIV_ST)
void x87_fdiv_ST(X87State *a1, unsigned int st_offset_1, unsigned int st_offset_2, bool pop_stack) {
	SIMDGuard simdGuard;

	LOG(1, "x87_fdiv_ST\n", 13);
	// Clear condition code 1 and exception flags
	a1->statusWord &= ~X87StatusWordFlag::kConditionCode1;

	// Get register indices and values
	const auto val1 = a1->getStFast(st_offset_1);
	const auto val2 = a1->getStFast(st_offset_2);

	// Perform division and store result
	a1->setStFast(st_offset_1, val1 / val2);

	if (pop_stack) {
		a1->pop();
	}
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fdiv_ST, (X87State * a1, unsigned int st_offset_1,  unsigned int st_offset_2, bool pop_stack), x9);
#endif

#if defined(X87_FDIV_F32)
void x87_fdiv_f32(X87State *a1, unsigned int a2) {
	SIMDGuard simdGuard;

	LOG(1, "x87_fdiv_f32\n", 14);
	a1->statusWord &= ~X87StatusWordFlag::kConditionCode1;

	auto value = std::bit_cast<float>(a2);
	auto st0 = a1->getStFast(0);

	a1->setStFast(0, st0 / value);
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fdiv_f32, (X87State * a1, unsigned int a2), x9);
#endif

#if defined(X87_FDIV_F64)
void x87_fdiv_f64(X87State *a1, unsigned long long a2) {
	SIMDGuard simdGuard;

	LOG(1, "x87_fdiv_f64\n", 14);

	a1->statusWord &= ~X87StatusWordFlag::kConditionCode1;

	auto value = std::bit_cast<double>(a2);
	auto st0 = a1->getStFast(0);

	a1->setStFast(0, st0 / value);
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fdiv_f64, (X87State * a1, unsigned long long a2), x9);
#endif

#if defined(X87_FDIVR_ST)
void x87_fdivr_ST(X87State *a1, unsigned int st_offset_1, unsigned int st_offset_2, bool pop_stack) {
	SIMDGuard simdGuard;

	LOG(1, "x87_fdivr_ST\n", 14);
	// Clear condition code 1 and exception flags
	a1->statusWord &= ~X87StatusWordFlag::kConditionCode1;

	// Get register indices and values
	const auto val1 = a1->getStFast(st_offset_1);
	const auto val2 = a1->getStFast(st_offset_2);

	// Perform reversed division and store result
	a1->setStFast(st_offset_1, val2 / val1);

	if (pop_stack) {
		a1->pop();
	}
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fdivr_ST, (X87State * a1, unsigned int st_offset_1, unsigned int st_offset_2, bool pop_stack), x9);
#endif

#if defined(X87_FDIVR_F32)
void x87_fdivr_f32(X87State *a1, unsigned int a2) {
	SIMDGuard simdGuard;

	LOG(1, "x87_fdivr_f32\n", 15);
	a1->statusWord &= ~X87StatusWordFlag::kConditionCode1;

	auto value = std::bit_cast<float>(a2);
	auto st0 = a1->getStFast(0);

	a1->setStFast(0, value / st0);
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fdivr_f32, (X87State * a1, unsigned int a2), x9);
#endif

#if defined(X87_FDIVR_F64)
void x87_fdivr_f64(X87State *a1, unsigned long long a2) {
	SIMDGuard simdGuard;

	LOG(1, "x87_fdivr_f64\n", 15);
	a1->statusWord &= ~X87StatusWordFlag::kConditionCode1;

	auto value = std::bit_cast<double>(a2);
	auto st0 = a1->getStFast(0);

	a1->setStFast(0, value / st0);
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fdivr_f64, (X87State * a1, unsigned long long a2), x9);
#endif

void x87_ffree(X87State *a1, unsigned int a2) {
	LOG(1, "x87_ffree\n", 11);
	orig_x87_ffree(a1, a2);
}

#if defined(X87_FIADD)
void x87_fiadd(X87State *a1, int m32int) {
	SIMDGuard simdGuard;

	LOG(1, "x87_fiadd\n", 11);
	// simplePrintf("m32int: %d\n", m32int);

	// Clear condition code 1 and exception flags
	a1->statusWord &= ~X87StatusWordFlag::kConditionCode1;

	// Get value in ST(0)
	auto st0 = a1->getSt(0);

	// Add integer value
	st0 += m32int;

	// Store result back in ST(0)
	a1->setSt(0, st0);
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fiadd, (X87State * a1, int m32int), x9);
#endif

#if defined(X87_FICOM)
void x87_ficom(X87State *a1, int src, bool pop) {
	SIMDGuard simdGuard;
	LOG(1, "x87_ficom\n", 11);
	auto st0 = a1->getSt(0);

	// Clear condition code bits C0, C2, C3 (bits 8, 9, 14)
	a1->statusWord &= ~(kConditionCode0 | kConditionCode2 | kConditionCode3);

	// Set condition codes based on comparison
	if (isnan(st0)) {
		a1->statusWord |= kConditionCode0 | kConditionCode2 | kConditionCode3; // Set C0=C2=C3=1
	} else if (st0 > src) {
		// Leave C0=C2=C3=0
	} else if (st0 < src) {
		a1->statusWord |= kConditionCode0; // Set C0=1
	} else {                                // st0 == src
		a1->statusWord |= kConditionCode3; // Set C3=1
	}

	// Handle pops if requested
	if (pop) {
		a1->pop();
	}
}
#else
X87_TRAMPOLINE_ARGS(void, x87_ficom, (X87State * a1, int src, bool pop), x9);
#endif

#if defined(X87_FIDIV)
void x87_fidiv(X87State *a1, int a2) {
	SIMDGuard simdGuard;

	LOG(1, "x87_fidiv\n", 11);
	// Clear condition code 1 and exception flags
	a1->statusWord &= ~X87StatusWordFlag::kConditionCode1;

	// Get value in ST(0)
	auto value = a1->getSt(0);

	// Divide by integer value
	value /= a2;

	// Store result back in ST(0)
	a1->setSt(0, value);
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fidiv, (X87State * a1, int a2), x9);
#endif

#if defined(X87_FIDIVR)
void x87_fidivr(X87State *a1, int a2) {
	SIMDGuard simdGuard;

	LOG(1, "x87_fidivr\n", 12);
	// Clear condition code 1 and exception flags
	a1->statusWord &= ~X87StatusWordFlag::kConditionCode1;

	// Get value in ST(0)
	auto value = a1->getSt(0);

	// Divide integer value by value in ST(0)
	value = a2 / value;

	// Store result back in ST(0)
	a1->setSt(0, value);
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fidivr, (X87State * a1, int a2), x9);
#endif

#if defined(X87_FILD)
void x87_fild(X87State *a1, int64_t value) {
	__asm__ volatile("" : : : "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7");
	SIMDGuard simdGuard;
	LOG(1, "x87_fild\n", 10);

	a1->push();
	a1->setSt(0, static_cast<double>(value));
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fild, (X87State * a1, int64_t value), x9);
#endif

#if defined(X87_FIMUL)
void x87_fimul(X87State *a1, int a2) {
	SIMDGuard simdGuard;
	LOG(1, "x87_fimul\n", 11);
	// Clear condition code 1 and exception flags
	a1->statusWord &= ~X87StatusWordFlag::kConditionCode1;

	// Get value in ST(0)
	auto value = a1->getSt(0);

	// Multiply by integer value
	value *= a2;

	// Store result back in ST(0)
	a1->setSt(0, value);
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fimul, (X87State * a1, int a2), x9);
#endif

void x87_fincstp(X87State *a1) {
	LOG(1, "x87_fincstp\n", 13);

	// Clear condition code 1 (C1)
	a1->statusWord &= ~X87StatusWordFlag::kConditionCode1;

	// Extract the TOP field (bits 11-13)
	uint16_t top = (a1->statusWord & X87StatusWordFlag::kTopOfStack) >> 11;

	// Increment TOP with wrap-around (values 0-7)
	top = (top + 1) & 0x7;

	// Clear old TOP value and set the new one
	a1->statusWord &= ~X87StatusWordFlag::kTopOfStack; // Clear TOP field
	a1->statusWord |= (top << 11);                     // Set new TOP value
}

#if defined(X87_FIST_I16)
X87ResultStatusWord x87_fist_i16(X87State const *a1) {
	SIMDGuard simdGuard;

	LOG(1, "x87_fist_i16\n", 14);
	auto [value, statusWord] = a1->getStConst(0);
	X87ResultStatusWord result{0, statusWord};

	// Special case: value > INT16_MAX or infinity (changed from >=)
	if (value > static_cast<double>(INT16_MAX)) {
		result.signedResult = INT16_MIN; // 0x8000
		result.statusWord |= X87StatusWordFlag::kConditionCode1;
		return result;
	}

	// Special case: value <= INT16_MIN
	if (value <= static_cast<double>(INT16_MIN)) {
		result.signedResult = INT16_MIN;
		result.statusWord |= X87StatusWordFlag::kConditionCode1;
		return result;
	}

	// Normal case
	auto round_bits = a1->controlWord & X87ControlWord::kRoundingControlMask;

	switch (round_bits) {
		case X87ControlWord::kRoundToNearest: {
			result.signedResult = static_cast<int16_t>(std::nearbyint(value));
		}
		break;

		case X87ControlWord::kRoundDown: {
			result.signedResult = static_cast<int16_t>(std::floor(value));
			return result;
		}
		break;

		case X87ControlWord::kRoundUp: {
			result.signedResult = static_cast<int16_t>(std::ceil(value));
			return result;
		}
		break;

		case X87ControlWord::kRoundToZero: {
			result.signedResult = static_cast<int16_t>(value);
			return result;
		}
		break;
	}

	return result;
}
#else
X87_TRAMPOLINE_ARGS(X87ResultStatusWord, x87_fist_i16, (X87State const *a1), x9);
#endif

#if defined(X87_FIST_I32)
X87ResultStatusWord x87_fist_i32(X87State const *a1) {
	SIMDGuard simdGuard;

	LOG(1, "x87_fist_i32\n", 14);
	auto [value, statusWord] = a1->getStConst(0);
	X87ResultStatusWord result{0, statusWord};

	// Special case: value >= INT32_MAX or infinity
	if (value >= static_cast<double>(INT32_MAX)) {
		result.signedResult = INT32_MIN; // 0x80000000
		result.statusWord |= X87StatusWordFlag::kConditionCode1;
		return result;
	}

	// Special case: value <= INT32_MIN
	if (value <= static_cast<double>(INT32_MIN)) {
		result.signedResult = INT32_MIN;
		result.statusWord |= X87StatusWordFlag::kConditionCode1;
		return result;
	}

	auto round_bits = a1->controlWord & X87ControlWord::kRoundingControlMask;

	switch (round_bits) {
		case X87ControlWord::kRoundToNearest: {
			result.signedResult = static_cast<int32_t>(std::nearbyint(value));
		}
		break;

		case X87ControlWord::kRoundDown: {
			result.signedResult = static_cast<int32_t>(std::floor(value));
			return result;
		}
		break;

		case X87ControlWord::kRoundUp: {
			result.signedResult = static_cast<int32_t>(std::ceil(value));
			return result;
		}
		break;

		case X87ControlWord::kRoundToZero: {
			result.signedResult = static_cast<int32_t>(value);
			return result;
		}
		break;
	}

	return result;
}
#else
X87_TRAMPOLINE_ARGS(X87ResultStatusWord, x87_fist_i32, (X87State const *a1), x9);
#endif

#if defined(X87_FIST_I64)
X87ResultStatusWord x87_fist_i64(X87State const *a1) {
	SIMDGuard simdGuard;

	LOG(1, "x87_fist_i64\n", 14);
	// Get value in ST(0)
	auto [value, statusWord] = a1->getStConst(0);

	X87ResultStatusWord result{0, statusWord};

	// Special case: value >= INT64_MAX or infinity
	if (value >= static_cast<double>(INT64_MAX)) {
		result.signedResult = INT64_MIN; // 0x8000000000000000
		result.statusWord |= X87StatusWordFlag::kConditionCode1;
		return result;
	}

	// Special case: value <= INT64_MIN
	if (value <= static_cast<double>(INT64_MIN)) {
		result.signedResult = INT64_MIN;
		result.statusWord |= X87StatusWordFlag::kConditionCode1;
		return result;
	}

	// Normal case

	auto round_bits = a1->controlWord & X87ControlWord::kRoundingControlMask;

	switch (round_bits) {
		case X87ControlWord::kRoundToNearest: {
			result.signedResult = static_cast<int64_t>(std::nearbyint(value));
		}
		break;

		case X87ControlWord::kRoundDown: {
			result.signedResult = static_cast<int64_t>(std::floor(value));
			return result;
		}
		break;

		case X87ControlWord::kRoundUp: {
			result.signedResult = static_cast<int64_t>(std::ceil(value));
			return result;
		}
		break;

		case X87ControlWord::kRoundToZero: {
			result.signedResult = static_cast<int64_t>(value);
			return result;
		}
		break;
	}

	return result;
}
#else
X87_TRAMPOLINE_ARGS(X87ResultStatusWord, x87_fist_i64, (X87State const *a1), x9);
#endif

#if defined(X87_FISTT_I16)
X87ResultStatusWord x87_fistt_i16(X87State const *a1) {
	SIMDGuard simdGuard;

	LOG(1, "x87_fistt_i16\n", 15);
	// Get value in ST(0)
	auto [value, statusWord] = a1->getStConst(0);

	return { .signedResult = static_cast<int16_t>(value), statusWord };
}
#else
X87_TRAMPOLINE_ARGS(X87ResultStatusWord, x87_fistt_i16, (X87State const *a1), x9);
#endif

#if defined(X87_FISTT_I32)
X87ResultStatusWord x87_fistt_i32(X87State const *a1) {
	SIMDGuard simdGuard;

	LOG(1, "x87_fistt_i32\n", 15);
	// Get value in ST(0)
	auto [value, statusWord] = a1->getStConst(0);

	return { .signedResult = static_cast<int32_t>(value), statusWord };
}
#else
X87_TRAMPOLINE_ARGS(X87ResultStatusWord, x87_fistt_i32, (X87State const *a1), x9);
#endif

#if defined(X87_FISTT_I64)
X87ResultStatusWord x87_fistt_i64(X87State const *a1) {
	SIMDGuard simdGuard;

	LOG(1, "x87_fistt_i64\n", 15);
	// Get value in ST(0)
	auto [value, statusWord] = a1->getStConst(0);

	return { .signedResult = static_cast<int64_t>(value), statusWord };
}
#else
X87_TRAMPOLINE_ARGS(X87ResultStatusWord, x87_fistt_i64, (X87State const *a1), x9);
#endif

#if defined(X87_FISUB)
void x87_fisub(X87State *a1, int a2) {
	SIMDGuard simdGuard;

	LOG(1, "x87_fisub\n", 11);
	// Clear condition code 1
	a1->statusWord &= ~(X87StatusWordFlag::kConditionCode1);

	// Get value in ST(0)
	auto value = a1->getSt(0);

	// Subtract integer value
	value -= a2;

	// Store result back in ST(0)
	a1->setSt(0, value);
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fisub, (X87State * a1, int a2), x9);
#endif

#if defined(X87_FISUBR)
void x87_fisubr(X87State *a1, int a2) {
	SIMDGuard simdGuard;

	LOG(1, "x87_fisubr\n", 12);

	// Clear condition code 1
	a1->statusWord &= ~(X87StatusWordFlag::kConditionCode1);

	// Get value in ST(0)
	auto value = a1->getSt(0);

	// Subtract integer value
	value = a2 - value;

	// Store result back in ST(0)
	a1->setSt(0, value);
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fisubr, (X87State * a1, int a2), x9);
#endif

// Push ST(i) onto the FPU register stack.
#if defined(X87_FLD_STI)
void x87_fld_STi(X87State *a1, unsigned int st_offset) {
	SIMDGuard simdGuard;

	LOG(1, "x87_fld_STi\n", 13);
	a1->statusWord &= ~0x200u;

	// Get index of ST(i) register
	const auto value = a1->getSt(st_offset);

	// make room for new value
	a1->push();

	// Copy value from ST(i) to ST(0)
	a1->setSt(0, value);
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fld_STi, (X87State * a1, unsigned int st_offset), x9);
#endif

#if defined(X87_FLD_CONSTANT)
void x87_fld_constant(X87State *a1, X87Constant a2) {
	SIMDGuard simdGuard;

	LOG(1, "x87_fld_constant\n", 18);
	// simplePrintf("x87_fld_constant %d\n", (int)a2);
	switch (a2) {
		case X87Constant::kOne: { // fld1
			a1->push();
			a1->setSt(0, 1.0);
		}
		break;

		case X87Constant::kZero: { // fldz
			a1->push();
			a1->setSt(0, 0.0);
		}
		break;

		case X87Constant::kPi: { // fldpi
			// store_x87_extended_value(a1, {.ieee754 = 3.141592741f});
			a1->push();
			a1->setSt(0, 3.141592741f);
		}
		break;

		case X87Constant::kLog2e: { // fldl2e
			// store_x87_extended_value(a1, {.ieee754 = 1.44269502f});
			a1->push();
			a1->setSt(0, 1.44269502f);
		}
		break;

		case X87Constant::kLoge2: { // fldln2
			// store_x87_extended_value(a1, {.ieee754 = 0.693147182f});
			a1->push();
			a1->setSt(0, 0.693147182f);
		}
		break;

		case X87Constant::kLog2t: { // fldl2t
			// store_x87_extended_value(a1, {.ieee754 = 3.321928f});
			a1->push();
			a1->setSt(0, 3.321928f);
		}
		break;

		case X87Constant::kLog102: { // fldl2e
			// store_x87_extended_value(a1, {.ieee754 = 0.301029987f});
			a1->push();
			a1->setSt(0, 0.301029987f);
		}
		break;

		default: {
			simplePrintf("x87_fld_constant ERROR %d\n", (int)a2);
		}
		break;
	}
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fld_constant, (X87State * a1, X87Constant a2), x9);
#endif

#if defined(X87_FLD_FP32)
void x87_fld_fp32(X87State *a1, unsigned int a2) {
	SIMDGuard simdGuard;

	LOG(1, "x87_fld_fp32\n", 14);

	// Push new value onto stack, get reference to new top
	a1->push();

	a1->setSt(0, std::bit_cast<float>(a2));
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fld_fp32, (X87State * a1, unsigned int a2), x9);
#endif

#if defined(X87_FLD_FP64)
void x87_fld_fp64(X87State *a1, unsigned long long a2) {
	SIMDGuard simdGuard;

	LOG(1, "x87_fld_fp64\n", 14);

	// Push new value onto stack, get reference to new top
	a1->push();

	a1->setSt(0, std::bit_cast<double>(a2));
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fld_fp64, (X87State * a1, unsigned long long a2), x9);
#endif

#if defined(X87_FLD_FP80)
void x87_fld_fp80(X87State *a1, X87Float80 a2) {
	LOG(1, "x87_fld_fp80\n", 14);

	auto ieee754 = ConvertX87RegisterToFloat64(a2, &a1->statusWord);

	a1->push();
	a1->setSt(0, ieee754);
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fld_fp80, (X87State * a1, X87Float80 a2), x9);
#endif

#if defined(X87_FMUL_ST)
void x87_fmul_ST(X87State *a1, unsigned int st_offset_1, unsigned int st_offset_2, bool pop_stack) {
	SIMDGuard simdGuard;

	LOG(1, "x87_fmul_ST\n", 13);

	// Clear condition code 1 and exception flags
	a1->statusWord &= ~X87StatusWordFlag::kConditionCode1;

	// Get register indices and values
	const auto val1 = a1->getStFast(st_offset_1);
	const auto val2 = a1->getStFast(st_offset_2);

	// Perform multiplication and store result
	a1->setStFast(st_offset_1, val1 * val2);

	if (pop_stack) {
		a1->pop();
	}
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fmul_ST, (X87State * a1, unsigned int st_offset_1, unsigned int st_offset_2, bool pop_stack), x9);
#endif

#if defined(X87_FMUL_F32)
void x87_fmul_f32(X87State *a1, unsigned int fp32) {
	SIMDGuard simdGuard;

	LOG(1, "x87_fmul_f32\n", 14);

	a1->statusWord &= ~X87StatusWordFlag::kConditionCode1;

	auto value = std::bit_cast<float>(fp32);
	auto st0 = a1->getStFast(0);

	a1->setStFast(0, st0 * value);
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fmul_f32, (X87State * a1, unsigned int fp32), x9);
#endif

#if defined(X87_FMUL_F64)
void x87_fmul_f64(X87State *a1, unsigned long long a2) {
	SIMDGuard simdGuard;

	LOG(1, "x87_fmul_f64\n", 14);

	a1->statusWord &= ~X87StatusWordFlag::kConditionCode1;

	auto value = std::bit_cast<double>(a2);
	auto st0 = a1->getStFast(0);

	a1->setStFast(0, st0 * value);
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fmul_f64, (X87State * a1, unsigned long long a2), x9);
#endif

// Replace ST(1) with arctan(ST(1)/ST(0)) and pop the register stack.
#if defined(X87_FPATAN)
void x87_fpatan(X87State *a1) {
	SIMDGuard simdGuard;

	LOG(1, "x87_fpatan\n", 12);

	a1->statusWord &= ~(X87StatusWordFlag::kConditionCode1);

	// Get values from ST(0) and ST(1)
	auto st0 = a1->getSt(0);
	auto st1 = a1->getSt(1);

	// Calculate arctan(ST(1)/ST(0))
	auto result = atan2(st1, st0);

	// Store result in ST(1) and pop the register stack
	a1->setSt(1, result);

	a1->pop();
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fpatan, (X87State * a1), x9);
#endif

#if defined(X87_FPREM)
void x87_fprem(X87State *a1) {
	SIMDGuard simdGuard;
	LOG(1, "x87_fprem\n", 11);

	// 1) Clear CC0–CC3
	a1->statusWord &=
		~(kConditionCode0 | kConditionCode1 | kConditionCode2 | kConditionCode3);

	double st0 = a1->getSt(0);
	double st1 = a1->getSt(1);

	// 2) Special cases: NaN/div0/∞ → #IA, ∞ divisor → pass through
	if (isnan(st0) || isnan(st1) || isinf(st0) || st1 == 0.0) {
		a1->setSt(0, std::numeric_limits<double>::quiet_NaN());
		a1->statusWord |= kInvalidOperation;
		return;
	}
	if (isinf(st1)) {
		// remainder = dividend; no exception
		return;
	}

	// 3) Compute truncated quotient and remainder
	double rawDiv = st0 / st1;
	double truncDiv = std::trunc(rawDiv); // Q = trunc(ST0/ST1)
	int q = static_cast<int>(truncDiv);
	double rem = std::fmod(st0, st1); // rem = ST0 - Q*ST1
	a1->setSt(0, rem);

	// 4) CC0, CC1, CC3 ← low bits of Q (Q2→CC0, Q0→CC1, Q1→CC3)
	if (q & 0x4)
		a1->statusWord |= kConditionCode0;
	if (q & 0x1)
		a1->statusWord |= kConditionCode1;
	if (q & 0x2)
		a1->statusWord |= kConditionCode3;

	// 5) CC2 “incomplete” if exponent gap > 0
	//    D = E0 – E1; E = std::ilogb(x)
	int e0 = std::ilogb(st0);
	int e1 = std::ilogb(st1);
	int D = e0 - e1;
	if (D > 0) {
		a1->statusWord |= kConditionCode2;
		// (optional) you could iterate: rem -= std::ldexp(trunc(rem/st1), D);
	}
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fprem, (X87State * a1), x9);
#endif

#if defined(X87_FPREM1)
void x87_fprem1(X87State *a1) {
	SIMDGuard simdGuard;
	LOG(1, "x87_fprem1\n", 12);

	// 1) clear condition-code bits CC0–CC3
	a1->statusWord &= ~(kConditionCode0 | kConditionCode1 | kConditionCode2 | kConditionCode3);

	double st0 = a1->getSt(0);
	double st1 = a1->getSt(1);

	// 2) special cases: NaN/div0/∞ → #IA or pass through
	if (isnan(st0) || isnan(st1) || isinf(st0) || st1 == 0.0) {
		a1->setSt(0, std::numeric_limits<double>::quiet_NaN());
		a1->statusWord |= kInvalidOperation;
		return;
	}
	if (isinf(st1)) {
		// remainder = dividend; no exception
		return;
	}

	// 3) IEEE-754 remainder with nearest-integer quotient
	int q;
	double rem = std::remquo(st0, st1, &q);
	// rem = ST0 – q*ST1, where q = round-to-nearest(ST0/ST1), ties-to-even
	a1->setSt(0, rem);

	// 4) CC0, CC1, CC3 from the three low bits of q:
	//    Q2→CC0, Q0→CC1, Q1→CC3
	if (q & 0x4)
		a1->statusWord |= kConditionCode0;
	if (q & 0x1)
		a1->statusWord |= kConditionCode1;
	if (q & 0x2)
		a1->statusWord |= kConditionCode3;

	// 5) CC2 = “incomplete” flag based on exponent diff D = E0 – E1
	int e0 = std::ilogb(st0); // unbiased exponent of st0
	int e1 = std::ilogb(st1); // unbiased exponent of st1
	int D = e0 - e1;
	if (D >= 64) {
		a1->statusWord |= kConditionCode2;
		// (optional) do the “partial” reduction loop per spec if you want
		// hardware-accurate step-wise remainder
	}
	// else D<64 ⇒ CC2 stays clear (complete reduction)
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fprem1, (X87State * a1), x9);
#endif

#if defined(X87_FPTAN)
void x87_fptan(X87State *a1) {
	SIMDGuard simdGuard;

	LOG(1, "x87_fptan\n", 11);

	a1->statusWord &= ~(X87StatusWordFlag::kConditionCode1 | X87StatusWordFlag::kConditionCode2);

	// Get value from ST(0)
	const auto value = a1->getSt(0);

	// Calculate tangent
	auto tan_value = tan(value);

	// Store result in ST(0)
	a1->setSt(0, tan_value);

	// Push 1.0 onto the FPU register stack
	a1->push();
	a1->setSt(0, 1.0);
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fptan, (X87State * a1), x9);
#endif

#if defined(X87_FRNDINT)
void x87_frndint(X87State *a1) {
	SIMDGuard simdGuard;

	LOG(1, "x87_frndint\n", 13);

	a1->statusWord &= ~(X87StatusWordFlag::kConditionCode1);

	// Get current value and round it
	double value = a1->getStFast(0);
	double rounded;
	auto round_bits = a1->controlWord & X87ControlWord::kRoundingControlMask;

	switch (round_bits) {
		case X87ControlWord::kRoundToNearest: {
			rounded = std::nearbyint(value);
		}
		break;

		case X87ControlWord::kRoundDown: {
			rounded = std::floor(value);
		}
		break;

		case X87ControlWord::kRoundUp: {
			rounded = std::ceil(value);
		}
		break;

		case X87ControlWord::kRoundToZero: {
			rounded = std::trunc(value);
		}
		break;
	}

	// Store rounded value and update tag
	a1->setStFast(0, rounded);
}
#else
X87_TRAMPOLINE_ARGS(void, x87_frndint, (X87State * a1), x9);
#endif

#if defined(X87_FSCALE)
void x87_fscale(X87State *state) {
	SIMDGuard simdGuard;

	LOG(1, "x87_fscale\n", 12);

	state->statusWord &= ~(X87StatusWordFlag::kConditionCode1);

	// Get values from ST(0) and ST(1)
	double st0 = state->getSt(0);
	double st1 = state->getSt(1);

	// Round ST(1) to nearest integer
	int scale = static_cast<int>(st1);

	// Scale ST(0) by 2^scale using bit manipulation for integer powers
	int32_t exponent = scale + 1023; // IEEE-754 bias
	uint64_t scaleFactor = static_cast<uint64_t>(exponent) << 52;
	double factor = std::bit_cast<double>(scaleFactor);

	// Multiply ST(0) by scale factor
	double result = st0 * factor;

	// Store result back in ST(0)
	state->setSt(0, result);
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fscale, (X87State * state), x9);
#endif

#if defined(X87_FSIN)
void x87_fsin(X87State *a1) {
	SIMDGuardFull simdGuard;

	LOG(1, "x87_fsin\n", 10);

	a1->statusWord &= ~(X87StatusWordFlag::kConditionCode1 | X87StatusWordFlag::kConditionCode2);

	// Get current value from top register
	const double value = a1->getStFast(0);

	// Store result and update tag
	a1->setStFast(0, sin(value));
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fsin, (X87State * a1), x9);
#endif

#if defined(X87_FSINCOS)
void x87_fsincos(X87State *a1) {
	SIMDGuardFull simdGuard;

	LOG(1, "x87_fsincos\n", 13);

	a1->statusWord &= ~(X87StatusWordFlag::kConditionCode1 | X87StatusWordFlag::kConditionCode2);

	// Get value from ST(0)
	const auto value = a1->getStFast(0);

	// Calculate sine and cosine
	auto sin_value = sin(value);
	auto cos_value = cos(value);

	// Store sine in ST(0)
	a1->setStFast(0, sin_value);

	// Push cosine onto the FPU register stack
	a1->push();
	a1->setStFast(0, cos_value);

	// Clear C2 condition code bit
	a1->statusWord &= ~X87StatusWordFlag::kConditionCode2;
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fsincos, (X87State * a1), x9);
#endif

// Computes square root of ST(0) and stores the result in ST(0).
#if defined(X87_FSQRT)
void x87_fsqrt(X87State *a1) {
	SIMDGuard simdGuard;

	LOG(1, "x87_fsqrt\n", 11);

	a1->statusWord &= ~(X87StatusWordFlag::kConditionCode1);

	// Get current value and calculate sqrt
	const double value = a1->getStFast(0);

	a1->statusWord |= X87StatusWordFlag::kPrecision;

	// Store result and update tag
	a1->setStFast(0, sqrt(value));
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fsqrt, (X87State * a1), x9);
#endif

#if defined(X87_FST_STI)
void x87_fst_STi(X87State *a1, unsigned int st_offset, bool pop) {
	SIMDGuard simdGuard;

	LOG(1, "x87_fst_STi\n", 13);

	// Clear C1 condition code (bit 9)
	a1->statusWord &= ~X87StatusWordFlag::kConditionCode1;

	// Copy ST(0) to ST(i)
	a1->setSt(st_offset, a1->getSt(0));

	// Pop if requested
	if (pop) {
		a1->pop();
	}
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fst_STi, (X87State * a1, unsigned int st_offset, bool pop), x9);
#endif

#if defined(X87_FST_FP32)
X87ResultStatusWord x87_fst_fp32(X87State const *a1) {
	SIMDGuard simdGuard;

	LOG(1, "x87_fst_fp32\n", 14);

	auto [value, statusWord] = a1->getStConst32(0);
	float tmp = value;
	return {std::bit_cast<uint32_t>(tmp), statusWord};
}
#else
X87_TRAMPOLINE_ARGS(X87ResultStatusWord, x87_fst_fp32, (X87State const *a1), x9);
#endif

#if defined(X87_FST_FP64)
X87ResultStatusWord x87_fst_fp64(X87State const *a1) {
	SIMDGuard simdGuard;

	LOG(1, "x87_fst_fp64\n", 14);

	// Create temporary double to ensure proper value representation
	auto [value, statusWord] = a1->getStConst(0);
	double tmp = value;
	return {std::bit_cast<uint64_t>(tmp), statusWord};
}
#else
X87_TRAMPOLINE_ARGS(X87ResultStatusWord, x87_fst_fp64, (X87State const *a1), x9);
#endif

#if defined(X87_FST_FP80)
X87Float80StatusWordResult x87_fst_fp80(X87State const *a1) {
	SIMDGuard simdGuard;

	LOG(1, "x87_fst_fp80\n", 14);

	// Get value from ST(0)
	auto [value, statusWord] = a1->getStConst(0);

	float tmp = value;
	uint32_t float32 = std::bit_cast<uint32_t>(tmp);

	// Extract components from float32
	uint32_t mantissa = float32 & 0x7FFFFF; // 23 bits
	uint8_t exp = (float32 >> 23) & 0xFF;   // 8 bits
	uint16_t sign = (float32 >> 31) << 15;  // Move sign to bit 15

	X87Float80StatusWordResult result;
	result.statusWord = statusWord;

	// Handle zero
	if (exp == 0 && mantissa == 0) {
		result.mantissa = 0;
		result.exponent = sign;
		return result;
	}

	// Handle subnormal numbers
	if (exp == 0) {
		// Set denormal flag

		// Count leading zeros to normalize
		int leading_zeros = __builtin_clz(mantissa) - 8; // -8 because mantissa is in upper 23 bits
		mantissa <<= leading_zeros;

		// Adjust exponent for normalization
		exp = 1 - leading_zeros;
	}
	// Handle infinity or NaN
	else if (exp == 255) {
		// Set invalid operation flag if NaN

		result.mantissa = (uint64_t)mantissa << 40 | 0x8000000000000000ULL;
		result.exponent = sign | 0x7FFF; // Maximum exponent
		return result;
	}

	// Normal numbers: Convert to x87 format
	// Shift 23-bit mantissa to 64 bits and set explicit integer bit
	result.mantissa = ((uint64_t)mantissa << 40) | 0x8000000000000000ULL;

	// Bias adjustment: IEEE 754 bias(127) to x87 bias(16383)
	result.exponent = sign | (exp + 16383 - 127);

	return result;
}
#else
X87_TRAMPOLINE_ARGS(X87Float80StatusWordResult, x87_fst_fp80, (X87State const *a1), x9);
#endif

#if defined(X87_FSUB_ST)
void x87_fsub_ST(X87State *a1, unsigned int st_offset1, unsigned int st_offset2, bool pop) {
	SIMDGuard simdGuard;

	LOG(1, "x87_fsub_ST\n", 13);

	// Clear condition code 1 and exception flags
	a1->statusWord &= ~X87StatusWordFlag::kConditionCode1;

	// Get register indices and values
	const auto val1 = a1->getStFast(st_offset1);
	const auto val2 = a1->getStFast(st_offset2);

	// Perform subtraction and store result in ST(st_offset1)
	a1->setStFast(st_offset1, val1 - val2);

	if (pop) {
		a1->pop();
	}
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fsub_ST, (X87State * a1, unsigned int st_offset1, unsigned int st_offset2, bool pop), x9);
#endif

#if defined(X87_FSUB_F32)
void x87_fsub_f32(X87State *a1, unsigned int a2) {
	SIMDGuard simdGuard;

	LOG(1, "x87_fsub_f32\n", 14);

	a1->statusWord &= ~X87StatusWordFlag::kConditionCode1;

	auto value = std::bit_cast<float>(a2);
	auto st0 = a1->getStFast(0);

	a1->setStFast(0, st0 - value);
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fsub_f32, (X87State * a1, unsigned int a2), x9);
#endif

#if defined(X87_FSUB_F64)
void x87_fsub_f64(X87State *a1, unsigned long long a2) {
	SIMDGuard simdGuard;

	LOG(1, "x87_fsub_f64\n", 14);

	a1->statusWord &= ~X87StatusWordFlag::kConditionCode1;

	auto value = std::bit_cast<double>(a2);
	auto st0 = a1->getStFast(0);

	a1->setStFast(0, st0 - value);
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fsub_f64, (X87State * a1, unsigned long long a2), x9);
#endif

#if defined(X87_FSUBR_ST)
void x87_fsubr_ST(X87State *a1, unsigned int st_offset1, unsigned int st_offset2, bool pop) {
	SIMDGuard simdGuard;

	LOG(1, "x87_fsubr_ST\n", 14);

	// Clear condition code 1 and exception flags
	a1->statusWord &= ~X87StatusWordFlag::kConditionCode1;

	// Get register indices and values
	const auto val1 = a1->getStFast(st_offset1);
	const auto val2 = a1->getStFast(st_offset2);

	// Perform reversed subtraction and store result in ST(st_offset1)
	a1->setStFast(st_offset1, val2 - val1);

	if (pop) {
		a1->pop();
	}
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fsubr_ST, (X87State * a1, unsigned int st_offset1, unsigned int st_offset2, bool pop), x9);
#endif

#if defined(X87_FSUBR_F32)
void x87_fsubr_f32(X87State *a1, unsigned int a2) {
	SIMDGuard simdGuard;

	LOG(1, "x87_fsubr_f32\n", 15);

	a1->statusWord &= ~X87StatusWordFlag::kConditionCode1;

	auto value = std::bit_cast<float>(a2);
	auto st0 = a1->getStFast(0);

	a1->setStFast(0, value - st0);
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fsubr_f32, (X87State * a1, unsigned int a2), x9);
#endif

#if defined(X87_FSUBR_F64)
void x87_fsubr_f64(X87State *a1, unsigned long long a2) {
	SIMDGuard simdGuard;

	LOG(1, "x87_fsubr_f64\n", 15);

	a1->statusWord &= ~X87StatusWordFlag::kConditionCode1;

	auto value = std::bit_cast<double>(a2);
	auto st0 = a1->getStFast(0);

	a1->setStFast(0, value - st0);
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fsubr_f64, (X87State * a1, unsigned long long a2), x9);
#endif

#if defined(X87_FUCOM)
void x87_fucom(X87State *a1, unsigned int st_offset, unsigned int pop) {
	SIMDGuard simdGuard;

	LOG(1, "x87_fucom\n", 11);
	auto st0 = a1->getSt(0);
	auto src = a1->getSt(st_offset);

	// Clear condition code bits C0, C2, C3 (bits 8, 9, 14)
	a1->statusWord &= ~(kConditionCode0 | kConditionCode2 | kConditionCode3);

	// Set condition codes based on comparison
	if (isnan(st0) || isnan(src)) {
		a1->statusWord |=
			kConditionCode0 | kConditionCode2 | kConditionCode3; // Set C0=C2=C3=1
	} else if (st0 > src) {
		// Leave C0=C2=C3=0
	} else if (st0 < src) {
		a1->statusWord |= kConditionCode0; // Set C0=1
	} else {                                // st0 == src
		a1->statusWord |= kConditionCode3; // Set C3=1
	}

	// Handle pops if requested
	for (auto i = 0; i < pop; ++i) {
		a1->pop();
	}
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fucom, (X87State * a1, unsigned int st_offset, unsigned int pop), x9);
#endif

#if defined(X87_FUCOMI)
uint32_t x87_fucomi(X87State *state, unsigned int st_offset, bool pop_stack) {
	SIMDGuard simdGuard;

	LOG(1, "x87_fucomi\n", 12);

	state->statusWord &= ~X87StatusWordFlag::kConditionCode1;

	auto st0_val = state->getSt(0);
	auto sti_val = state->getSt(st_offset);

	uint32_t flags = 0;
	/*
	Filters: fcomi
	Randomness seeded to: 3528984885
	x87_fcomi_less
	x87_fcomi result: 0x000000000000000
	x87_fcomi_greater
	x87_fcomi result: 0x000000020000000
	x87_fcomi_equal
	x87_fcomi result: 0x000000060000000
	*/

	if (st0_val < sti_val) {
		flags = 0x000000000000000;
	} else if (st0_val > sti_val) {
		flags = 0x000000020000000;
	} else {
		flags = 0x000000060000000;
	}

	if (pop_stack) {
		state->pop();
	}

	return flags;
}
#else
X87_TRAMPOLINE_ARGS(uint32_t, x87_fucomi, (X87State * state, unsigned int st_offset, bool pop_stack), x9);
#endif

#if defined(X87_FXAM)
void x87_fxam(X87State *a1) {
	SIMDGuard simdGuard;

	LOG(1, "x87_fxam\n", 10);

	// Get tag state for ST(0)
	X87TagState tag = a1->getStTag(0);

	// simplePrintf("tag: %d\n", tag);

	static_assert((X87StatusWordFlag::kConditionCode0 | X87StatusWordFlag::kConditionCode1 | X87StatusWordFlag::kConditionCode2 | X87StatusWordFlag::kConditionCode3) == 0x4700);

	// Clear C3,C2,C1,C0 bits
	a1->statusWord &= ~(
		X87StatusWordFlag::kConditionCode0 | X87StatusWordFlag::kConditionCode1 |
		X87StatusWordFlag::kConditionCode2 | X87StatusWordFlag::kConditionCode3);

	// Handle empty and zero based on tag word
	if (tag == X87TagState::kEmpty) {
		a1->statusWord |= X87StatusWordFlag::kConditionCode3 | X87StatusWordFlag::kConditionCode0; // C3=1, C0=1 (101)
		return;
	}
	if (tag == X87TagState::kZero) {
		a1->statusWord |= X87StatusWordFlag::kConditionCode3; // C3=1 (100)
		return;
	}

	// Get actual value for other cases
	auto value = a1->getSt(0);

	// Set C1 based on sign
	if (signbit(value)) {
		a1->statusWord |= X87StatusWordFlag::kConditionCode1;
	}

	// Set C3,C2,C0 based on value type
	if (isnan(value)) {
		a1->statusWord |= X87StatusWordFlag::kConditionCode0; // 001
	} else if (isinf(value)) {
		a1->statusWord |= X87StatusWordFlag::kConditionCode2 | X87StatusWordFlag::kConditionCode0; // 011
	} else if (fpclassify(value) == FP_SUBNORMAL) {
		a1->statusWord |= X87StatusWordFlag::kConditionCode3 | X87StatusWordFlag::kConditionCode2; // 110
	} else {
		a1->statusWord |= X87StatusWordFlag::kConditionCode2; // 010 (normal)
	}
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fxam, (X87State * a1), x9);
#endif

#if defined(X87_FXCH)
void x87_fxch(X87State *a1, unsigned int st_offset) {
	SIMDGuard simdGuard;

	LOG(1, "x87_fxch\n", 10);

	// Clear condition code 1
	a1->statusWord &= ~X87StatusWordFlag::kConditionCode1;

	auto st0 = a1->getSt(0);
	auto sti = a1->getSt(st_offset);

	a1->setSt(0, sti);
	a1->setSt(st_offset, st0);
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fxch, (X87State * a1, unsigned int st_offset), x9);
#endif

#if defined(X87_FXTRACT)
void x87_fxtract(X87State *a1) {
	SIMDGuardFull simdGuard;

	LOG(1, "x87_fxtract\n", 13);

	auto st0 = a1->getSt(0);

	// If the floating-point zero-divide exception (#Z) is masked and the source
	// operand is zero, an exponent value of –∞ is stored in register ST(1) and 0
	// with the sign of the source operand is stored in register ST(0).
	if ((a1->controlWord & X87ControlWord::kZeroDivideMask) != 0 && st0 == 0.0) {
		a1->setSt(1, -std::numeric_limits<double>::infinity());
		a1->setSt(0, copysign(0.0, st0));
		return;
	}

	if (isinf(st0)) {
		a1->setSt(0, st0);
		a1->push();
		a1->setSt(0, std::numeric_limits<double>::infinity());
		return;
	}

	auto e = std::floor(log2(abs(st0)));
	auto m = st0 / pow(2.0, e);

	a1->setSt(0, e);

	a1->push();
	a1->setSt(0, m);
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fxtract, (X87State * a1), x9);
#endif

void fyl2x_common(X87State *state, double constant) {
	// Clear condition code 1
	state->statusWord &= ~X87StatusWordFlag::kConditionCode1;

	// Get x from ST(0) and y from ST(1)
	auto st0 = state->getSt(0);
	auto st1 = state->getSt(1);

	// Calculate y * log2(x)
	auto result = st1 * (log2(st0 + constant));

	// Pop ST(0)
	state->pop();

	// Store result in new ST(0)
	state->setSt(0, result);
}

// Replace ST(1) with (ST(1) ∗ log2ST(0)) and pop the register stack.
#if defined(X87_FYL2X)
void x87_fyl2x(X87State *state) {
	SIMDGuardFull simdGuard;
	LOG(1, "x87_fyl2x\n", 12);

	fyl2x_common(state, 0.0);
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fyl2x, (X87State * state), x9);
#endif

// Replace ST(1) with (ST(1) ∗ log2ST(0 + 1.0)) and pop the register stack.
#if defined(X87_FYL2XP1)
void x87_fyl2xp1(X87State *state) {
	SIMDGuardFull simdGuard;
	LOG(1, "x87_fyl2xp1\n", 14);

	fyl2x_common(state, 1.0);
}
#else
X87_TRAMPOLINE_ARGS(void, x87_fyl2xp1, (X87State * state), x9);
#endif

X87_TRAMPOLINE(sse_pcmpestri, x9)
X87_TRAMPOLINE(sse_pcmpestrm, x9)
X87_TRAMPOLINE(sse_pcmpistri, x9)
X87_TRAMPOLINE(sse_pcmpistrm, x9)
X87_TRAMPOLINE(is_ldt_initialized, x9)
X87_TRAMPOLINE(get_ldt, x9)
X87_TRAMPOLINE(set_ldt, x9)
X87_TRAMPOLINE(execution_mode_for_code_segment_selector, x9)
X87_TRAMPOLINE(mov_segment, x9)
X87_TRAMPOLINE(abi_for_address, x9)

X87_TRAMPOLINE(determine_state_recovery_action, x9)
X87_TRAMPOLINE(get_segment_limit, x9)
X87_TRAMPOLINE(translator_set_variant, x9)

#if defined(X87_CONVERT_TO_FP80)
X87_TRAMPOLINE_ARGS(void, x87_set_init_state, (X87State *state), x9);
#else
void x87_set_init_state(X87State *state) {
	SIMDGuard simdGuard;
	LOG(1, "x87_set_init_state\n", 9);

	state->controlWord = 0x037F;
	state->statusWord = 0x0000;
	state->tagWord = 0xFFFF;
	for (int i = 0; i < 8; i++) {
		state->st[i].ieee754 = 0.0;
	}
}
#endif

X87_TRAMPOLINE(runtime_cpuid, x22)
X87_TRAMPOLINE(runtime_wide_udiv_64, x9)
X87_TRAMPOLINE(runtime_wide_sdiv_64, x9)
