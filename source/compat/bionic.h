#ifndef DQIVN_BIONIC_H
#define DQIVN_BIONIC_H
#include <stddef.h>
#include <stdint.h>
#include <time.h>
#ifdef __cplusplus
extern "C" {
#endif
int  __cxa_guard_acquire_soloader(volatile uint32_t *g);
void __cxa_guard_release_soloader(volatile uint32_t *g);
void __cxa_guard_abort_soloader(volatile uint32_t *g);
int  __cxa_atexit_soloader(void (*f)(void *), void *arg, void *dso);
int  __aeabi_atexit_soloader(void *obj, void (*dtor)(void *), void *dso);
void __cxa_finalize_soloader(void *dso);
void __cxa_pure_virtual_soloader(void);
void __stack_chk_fail_soloader(void);
void abort_soloader(void);
int  raise_soloader(int sig);
clock_t clock_soloader(void);
extern int __page_size_soloader;
int __android_log_write(int prio, const char *tag, const char *text);
int __android_log_print(int prio, const char *tag, const char *fmt, ...);
extern const char *BIONIC_ctype_;
extern const short *BIONIC_tolower_tab_;
extern const short *BIONIC_toupper_tab_;
#ifdef __cplusplus
}
#endif
#endif
