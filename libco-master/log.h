#ifndef __LOG_H__
#define __LOG_H__


extern "C" {
#include <stdio.h>
#include <stdarg.h>
#include <libgen.h>
#include <string.h>
}
#define LOG 1

void info(const char*file, const int line, const char*fmt,...);

#define INFO(...) { \
		info(__FILE__, __LINE__, __VA_ARGS__); \
}

#endif
