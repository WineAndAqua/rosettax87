#pragma once

#include <cstdarg>
#include <cstdint>

// flip this to 1 to enable logging of instruction calls
#if 0
#define LOG(fd, msg, len) syscall_write(1, msg, len);
//syscall_write(1, msg, len);
//#define MISSING(fd, msg, len) ((void)0)
#else
#define LOG(fd, msg, len) ((void)0)
#endif
#ifndef MISSING
#define MISSING(fd, msg, len) syscall_write(1, msg, len);
#endif

extern auto syscall_write(int fd, const char *buf, uint64_t count) -> uint64_t;

extern void simple_printf(const char *format, ...);
