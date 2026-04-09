#pragma once

#include <cstdarg>
#include <cstdint>
#include <unistd.h>

// flip this to 1 to enable logging of instruction calls
#if 0
#define LOG(fd, msg, len) syscallWrite(STDERR_FILENO, msg, len);
//syscallWrite(1, msg, len);
//#define MISSING(fd, msg, len) ((void)0)
#else
#define LOG(fd, msg, len) ((void)0)
#endif
#ifndef MISSING
#define MISSING(fd, msg, len) syscallWrite(STDERR_FILENO, msg, len);
#endif

extern auto syscallWrite(int fd, const char *buf, uint64_t count) -> uint64_t;

extern void simplePrintf(const char *format, ...);
