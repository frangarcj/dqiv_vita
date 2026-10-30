// host shim: just enough of the Vita SDK for engine_core.cpp to build on macOS/Linux
#pragma once
#include <stdint.h>
#include <time.h>
static inline uint64_t sceKernelGetProcessTimeWide(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return (uint64_t)ts.tv_sec * 1000000ull + ts.tv_nsec / 1000; }
