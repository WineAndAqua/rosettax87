#include "Log.h"
#include <cmath>

auto syscall_write(int fd, const char *buf, uint64_t count) -> uint64_t {
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

__attribute__((no_stack_protector, optnone)) void simple_printf(const char *format, ...) {
	static char buffer[1024];
	char *buf_ptr = buffer;
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
						*buf_ptr++ = *nan++;
					break;
				}
				if (std::isinf(f)) {
					const char *inf = "inf";
					while (*inf)
						*buf_ptr++ = *inf++;
					break;
				}

				// Handle negative numbers
				if (f < 0) {
					*buf_ptr++ = '-';
					f = -f;
				}

				// Extract integer and fractional parts
				int64_t integer_part = (int64_t)f;
				double fractional_part = f - integer_part;

				// Print integer part
				char int_buf[20];
				char *int_ptr = int_buf + sizeof(int_buf) - 1;
				*int_ptr = '\0';

				do {
					*--int_ptr = '0' + (integer_part % 10);
					integer_part /= 10;
				} while (integer_part > 0);

				while (*int_ptr)
					*buf_ptr++ = *int_ptr++;

				// Print decimal point and fractional part
				*buf_ptr++ = '.';

				// Print 6 decimal places
				int precision = 6;
				while (precision-- > 0) {
					fractional_part *= 10;
					int digit = (int)fractional_part;
					*buf_ptr++ = '0' + digit;
					fractional_part -= digit;
				}
				break;
			}
			case 's':
				str = va_arg(args, const char *);
				while (*str != '\0') {
					*buf_ptr++ = *str++;
				}
				break;
			case 'p': {
				uint64_t p = (uint64_t)va_arg(args, void *);
				static char num_buf[18]; // 0x + 16 digits + null
				char *num_ptr = num_buf + sizeof(num_buf) - 1;
				*num_ptr = '\0';
				*buf_ptr++ = '0';
				*buf_ptr++ = 'x';
				do {
					int digit = p & 0xF;
					*--num_ptr = digit < 10 ? '0' + digit : 'a' + (digit - 10);
					p >>= 4;
				} while (p != 0);
				// // Pad with zeros to ensure 16 digits
				while (num_ptr > num_buf + 2) {
					*--num_ptr = '0';
				}
				while (*num_ptr != '\0') {
					*buf_ptr++ = *num_ptr++;
				}
				break;
			}
			case 'd': {
				d = va_arg(args, int);
				char num_buf[20];
				char *num_ptr = num_buf + sizeof(num_buf) - 1;
				*num_ptr = '\0';
				if (d < 0) {
					*buf_ptr++ = '-';
					d = -d;
				}
				do {
					*--num_ptr = '0' + (d % 10);
					d /= 10;
				} while (d != 0);
				while (*num_ptr != '\0') {
					*buf_ptr++ = *num_ptr++;
				}
				break;
			}
			case 'l': {
				++ptr; // Skip 'l'
				if (*ptr == 'd') {
					d = va_arg(args, long long);
					char num_buf[20];
					char *num_ptr = num_buf + sizeof(num_buf) - 1;
					*num_ptr = '\0';
					if (d < 0) {
						*buf_ptr++ = '-';
						d = -d;
					}
					do {
						*--num_ptr = '0' + (d % 10);
						d /= 10;
					} while (d != 0);
					while (*num_ptr != '\0') {
						*buf_ptr++ = *num_ptr++;
					}
				}
				break;
			}
			default:
				*buf_ptr++ = '%';
				*buf_ptr++ = *ptr;
				break;
			}
		} else {
			*buf_ptr++ = *ptr;
		}
	}

	*buf_ptr = '\0';
	va_end(args);

	syscall_write(1, buffer, buf_ptr - buffer);
}
