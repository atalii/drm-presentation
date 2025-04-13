#pragma once

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STR_ERR (strerror(errno))

#define FATAL_ERR(str, ...)                                            \
	do {                                                           \
		fprintf(stderr,                                        \
		    "\033[31;1mFATAL: " str "\033[0m\n" __VA_OPT__(, ) \
			__VA_ARGS__);                                  \
		exit(1);                                               \
	} while (0);

#define LOG_STAGE(str) \
	do { \
		fprintf(stderr, "\n\033[1m" str "\n\033[0m"); \
	} while (0); 
