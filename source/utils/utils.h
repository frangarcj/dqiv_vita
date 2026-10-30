#ifndef DQIVN_UTILS_H
#define DQIVN_UTILS_H
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#ifdef __cplusplus
extern "C" {
#endif
bool file_exists(const char *path);
bool file_mkpath(const char *path, mode_t mode);   /* mkdir -p */
bool module_loaded(const char *name);              /* taiHEN/kernel module by name */
void fatal_error(const char *fmt, ...) __attribute__((noreturn, format(printf, 1, 2)));
#ifdef __cplusplus
}
#endif
#endif
