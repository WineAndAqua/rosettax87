#include "Log.h"

#include <cmath>

auto syscallWrite(int fd, const char *buf, uint64_t count) -> uint64_t {
	register uint64_t x0 __asm__("x0") = fd;
	register uint64_t x1 __asm__("x1") = (uint64_t)buf;
	register uint64_t x2 __asm__("x2") = count;
	register uint64_t x16 __asm__("x16") = 397; // SYS_write_nocancel

	asm volatile(
		"svc #0x80\n"
		"mov x1, #-1\n"
		"csel x0, x1, x0, cs\n"
		: "+r"(x0)
		: "r"(x1), "r"(x2), "r"(x16)
		: "memory");

	return x0;
}

__attribute__((no_stack_protector, optnone)) void simplePrintf(const char *format, ...) {
	static char buffer[1024];
	char *bufPtr = buffer;
	const char *str;
	int d;
	va_list args;
	va_start(args, format);

	buffer[0] = '\0';

	for (const char *ptr = format; *ptr != '\0'; ++ptr) {
		if (*ptr == '%' && *(ptr + 1) != '\0') {
			++ptr;
			switch (*ptr) {
			case 'f': {
				double f = va_arg(args, double);

				// Handle special cases
				if (std::isnan(f)) {
					const char *nan = "nan";
					while (*nan)
						*bufPtr++ = *nan++;
					break;
				}
				if (std::isinf(f)) {
					const char *inf = "inf";
					while (*inf)
						*bufPtr++ = *inf++;
					break;
				}

				// Handle negative numbers
				if (f < 0) {
					*bufPtr++ = '-';
					f = -f;
				}

				// Extract integer and fractional parts
				int64_t integerPart = (int64_t)f;
				double fractionalPart = f - integerPart;

				// Print integer part
				char intBuf[20];
				char *intPtr = intBuf + sizeof(intBuf) - 1;
				*intPtr = '\0';

				do {
					*--intPtr = '0' + (integerPart % 10);
					integerPart /= 10;
				} while (integerPart > 0);

				while (*intPtr)
					*bufPtr++ = *intPtr++;

				// Print decimal point and fractional part
				*bufPtr++ = '.';

				// Print 6 decimal places
				int precision = 6;
				while (precision-- > 0) {
					fractionalPart *= 10;
					int digit = (int)fractionalPart;
					*bufPtr++ = '0' + digit;
					fractionalPart -= digit;
				}
				break;
			}
			case 's':
				str = va_arg(args, const char *);
				while (*str != '\0') {
					*bufPtr++ = *str++;
				}
				break;
			case 'p': {
				uint64_t p = (uint64_t)va_arg(args, void *);
				static char numBuf[18]; // 0x + 16 digits + null
				char *numPtr = numBuf + sizeof(numBuf) - 1;
				*numPtr = '\0';
				*bufPtr++ = '0';
				*bufPtr++ = 'x';
				do {
					int digit = p & 0xF;
					*--numPtr = digit < 10 ? '0' + digit : 'a' + (digit - 10);
					p >>= 4;
				} while (p != 0);
				// Pad with zeros to ensure 16 digits
				while (numPtr > numBuf + 2) {
					*--numPtr = '0';
				}
				while (*numPtr != '\0') {
					*bufPtr++ = *numPtr++;
				}
				break;
			}
			case 'd': {
				d = va_arg(args, int);
				char numBuf[20];
				char *numPtr = numBuf + sizeof(numBuf) - 1;
				*numPtr = '\0';
				if (d < 0) {
					*bufPtr++ = '-';
					d = -d;
				}
				do {
					*--numPtr = '0' + (d % 10);
					d /= 10;
				} while (d != 0);
				while (*numPtr != '\0') {
					*bufPtr++ = *numPtr++;
				}
				break;
			}
			case 'l': {
				++ptr; // Skip 'l'
				if (*ptr == 'd') {
					d = va_arg(args, long long);
					char numBuf[20];
					char *numPtr = numBuf + sizeof(numBuf) - 1;
					*numPtr = '\0';
					if (d < 0) {
						*bufPtr++ = '-';
						d = -d;
					}
					do {
						*--numPtr = '0' + (d % 10);
						d /= 10;
					} while (d != 0);
					while (*numPtr != '\0') {
						*bufPtr++ = *numPtr++;
					}
				}
				break;
			}
			default:
				*bufPtr++ = '%';
				*bufPtr++ = *ptr;
				break;
			}
		} else {
			*bufPtr++ = *ptr;
		}
	}

	*bufPtr = '\0';
	va_end(args);

	syscallWrite(1, buffer, bufPtr - buffer);
}
