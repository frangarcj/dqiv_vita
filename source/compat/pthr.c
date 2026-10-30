/*
 * Copyright (C) 2021      Andy Nguyen
 * Copyright (C) 2022      Rinnegatamante
 * Copyright (C) 2022      GrapheneCt
 * Copyright (C) 2022-2024 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

/*
 * bionic <-> newlib/pthread-embedded bridge. Differences from the He-Man port:
 *   - no registry of initialised objects: the bionic mutex/cond word either holds a static
 *     initialiser (< 0x10000) or a pointer to the real object (lazy init under a global lock)
 *   - bigger default thread stacks (Mono / Unity threads), pthread_getattr_np/getstack for the GC
 *   - __pthread_cleanup_push/pop, sched_get_priority_*, pthread_sigmask, monotonic cond wait
 */

#include "compat/pthr.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <psp2/kernel/clib.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>

#include "utils/logger.h"

#define BIONIC_PTHREAD_RECURSIVE_MUTEX_INITIALIZER   0x4000
#define BIONIC_PTHREAD_ERRORCHECK_MUTEX_INITIALIZER  0x8000

#define DEFAULT_THREAD_STACK (1024 * 1024)
#define MIN_THREAD_STACK     (128 * 1024)
#define MAX_THREAD_STACK     (16 * 1024 * 1024)

#define IS_STATIC_INIT(p) ((uintptr_t)(p) < 0x10000)

/* One global lock serialises the lazy creation of mutexes/conds/attrs (cheap: only on first use). */
static SceKernelLwMutexWork init_lock;
static volatile int init_lock_ready = 0;
static SceKernelLwMutexWork *get_init_lock(void) {
    if (!init_lock_ready) {
        /* Created from a single thread before any contention in practice; make it race-safe anyway. */
        static volatile int creating = 0;
        if (__sync_lock_test_and_set(&creating, 1) == 0) {
            sceKernelCreateLwMutex(&init_lock, "pthr_init", SCE_KERNEL_MUTEX_ATTR_RECURSIVE, 0, NULL);
            init_lock_ready = 1;
        } else {
            while (!init_lock_ready) sceKernelDelayThread(100);
        }
    }
    return &init_lock;
}

/* ------------------------------------------------------------------------------------------ */
/* attrs                                                                                       */
/* ------------------------------------------------------------------------------------------ */

static int attr_ensure(pthread_attr_t_bionic *attr) {
    if (attr->magic == 0x42424242 && attr->real_ptr) return 0;
    attr->real_ptr = malloc(sizeof(pthread_attr_t));
    if (!attr->real_ptr) return ENOMEM;
    attr->magic = 0x42424242;
    attr->stack_size = DEFAULT_THREAD_STACK;
    attr->stack_base = NULL;
    attr->sched_policy = 0;
    attr->sched_priority = 0;
    return pthread_attr_init(attr->real_ptr);
}

int pthread_attr_init_soloader(pthread_attr_t_bionic *attr) {
    if (!attr) return EINVAL;
    attr->magic = 0;           /* a fresh attr may contain garbage */
    attr->real_ptr = NULL;
    return attr_ensure(attr);
}

int pthread_attr_destroy_soloader(pthread_attr_t_bionic *attr) {
    if (!attr) return 0;
    if (attr->magic != 0x42424242) return 0;
    int ret = pthread_attr_destroy(attr->real_ptr);
    free(attr->real_ptr);
    attr->real_ptr = NULL;
    attr->magic = 0;
    return ret;
}

int pthread_attr_setdetachstate_soloader(pthread_attr_t_bionic *attr, int state) {
    if (!attr) return EINVAL;
    attr_ensure(attr);
    /* bionic: JOINABLE=0, DETACHED=1; pthread-embedded has them swapped */
    return pthread_attr_setdetachstate(attr->real_ptr, state ? PTHREAD_CREATE_DETACHED : PTHREAD_CREATE_JOINABLE);
}

int pthread_attr_getdetachstate_soloader(const pthread_attr_t_bionic *attr, int *state) {
    if (!attr || !state) return EINVAL;
    int s = 0;
    attr_ensure((pthread_attr_t_bionic *)attr);
    pthread_attr_getdetachstate(attr->real_ptr, &s);
    *state = (s == PTHREAD_CREATE_DETACHED) ? 1 : 0;
    return 0;
}

int pthread_attr_setstacksize_soloader(pthread_attr_t_bionic *attr, size_t stacksize) {
    if (!attr) return EINVAL;
    attr_ensure(attr);
    attr->stack_size = stacksize;
    return pthread_attr_setstacksize(attr->real_ptr, stacksize < MIN_THREAD_STACK ? MIN_THREAD_STACK : stacksize);
}

int pthread_attr_getstack_soloader(const pthread_attr_t_bionic *attr, void **stackaddr, size_t *stacksize) {
    if (!attr) return EINVAL;
    if (stackaddr) *stackaddr = attr->stack_base;
    if (stacksize) *stacksize = attr->stack_size;
    return 0;
}

int pthread_attr_setschedparam_soloader(pthread_attr_t_bionic *attr, const struct sched_param *param) {
    if (!attr || !param) return EINVAL;
    attr->sched_priority = param->sched_priority;   /* recorded only; Vita priorities are left alone */
    return 0;
}

int pthread_attr_setschedpolicy_soloader(pthread_attr_t_bionic *attr, int policy) {
    if (!attr) return EINVAL;
    attr->sched_policy = policy;
    return 0;
}

/* No GC here: report the attr as is (sdlib never calls it, kept for completeness). */
int pthread_getattr_np_soloader(pthread_t thread, pthread_attr_t_bionic *attr) {
    (void)thread;
    if (!attr) return EINVAL;
    pthread_attr_init_soloader(attr);
    attr->stack_base = NULL;
    attr->stack_size = DEFAULT_THREAD_STACK;
    return 0;
}

/* ------------------------------------------------------------------------------------------ */
/* threads                                                                                     */
/* ------------------------------------------------------------------------------------------ */

int pthread_create_soloader(pthread_t *thread, const pthread_attr_t_bionic *attr, void *(*start)(void *), void *param) {
    int ret;
    size_t stack = DEFAULT_THREAD_STACK;

    if (!attr || attr->magic != 0x42424242 || !attr->real_ptr) {
        pthread_attr_t a;
        pthread_attr_init(&a);
        pthread_attr_setstacksize(&a, stack);
        ret = pthread_create(thread, &a, start, param);
        pthread_attr_destroy(&a);
    } else {
        stack = attr->stack_size;
        if (stack < MIN_THREAD_STACK) stack = MIN_THREAD_STACK;
        if (stack > MAX_THREAD_STACK) stack = MAX_THREAD_STACK;
        pthread_attr_setstacksize(attr->real_ptr, stack);
        ret = pthread_create(thread, attr->real_ptr, start, param);
    }
    l_debug("pthread_create(start=%p stack=%u) -> %d thread=%p", start, (unsigned)stack, ret, thread ? (void *)*thread : NULL);
    return ret;
}

int pthread_join_soloader(pthread_t thread, void **value_ptr) { return pthread_join(thread, value_ptr); }
int pthread_detach_soloader(pthread_t thread) { return pthread_detach(thread); }
pthread_t pthread_self_soloader() { return pthread_self(); }

int pthread_equal_soloader(const pthread_t t1, const pthread_t t2) {
    if (t1 == t2) return 1;
    if (!t1 || !t2) return 0;
    return pthread_equal(t1, t2);
}

int pthread_setschedparam_soloader(pthread_t thread, int policy, const struct sched_param *param) {
    (void)thread; (void)policy; (void)param;
    return 0;   /* keep the Vita thread priorities untouched */
}

int pthread_getschedparam_soloader(pthread_t thread, int *policy, struct sched_param *param) {
    (void)thread;
    if (policy) *policy = 0;
    if (param) param->sched_priority = 0;
    return 0;
}

int sched_get_priority_max_soloader(int policy) { (void)policy; return 99; }
int sched_get_priority_min_soloader(int policy) { (void)policy; return (policy == 0) ? 0 : 1; }

int pthread_once_soloader(volatile int *once_control, void (*init_routine)(void)) {
    if (!once_control || !init_routine) return EINVAL;
    /* states: 0 = not run, 1 = running, 2 = done */
    if (*once_control == 2) return 0;
    if (__sync_bool_compare_and_swap(once_control, 0, 1)) {
        init_routine();
        __sync_synchronize();
        *once_control = 2;
    } else {
        while (*once_control != 2) sceKernelDelayThread(100);
    }
    return 0;
}

#ifndef MAX_TASK_COMM_LEN
#define MAX_TASK_COMM_LEN 16
#endif

int pthread_setname_np_soloader(pthread_t thread, const char *thread_name) {
    if (thread == 0 || thread_name == NULL) return EINVAL;
    if (strlen(thread_name) >= MAX_TASK_COMM_LEN) return ERANGE;
    l_debug("pthread_setname_np(%s)", thread_name);
    return 0;
}

/* ------------------------------------------------------------------------------------------ */
/* mutexes                                                                                     */
/* ------------------------------------------------------------------------------------------ */

int pthread_mutexattr_init_soloader(pthread_mutexattr_t *attr) { return pthread_mutexattr_init(attr); }
int pthread_mutexattr_settype_soloader(pthread_mutexattr_t *attr, int type) { return pthread_mutexattr_settype(attr, type); }
int pthread_mutexattr_destroy_soloader(pthread_mutexattr_t *attr) { return pthread_mutexattr_destroy(attr); }

static int mutex_alloc(pthread_mutex_t_bionic *m, int kind) {
    pthread_mutex_t *real = malloc(sizeof(pthread_mutex_t));
    if (!real) return ENOMEM;
    pthread_mutexattr_t a;
    pthread_mutexattr_init(&a);
    pthread_mutexattr_settype(&a, kind);
    int ret = pthread_mutex_init(real, &a);
    pthread_mutexattr_destroy(&a);
    if (ret != 0) { free(real); l_error("mutex init for %p failed: %d", m, ret); return ret; }
    m->real_ptr = real;
    return 0;
}

/* Returns the real mutex, creating it for static initialisers. */
static pthread_mutex_t *mutex_get(pthread_mutex_t_bionic *m) {
    if (!IS_STATIC_INIT(m->real_ptr)) return m->real_ptr;

    SceKernelLwMutexWork *lk = get_init_lock();
    sceKernelLockLwMutex(lk, 1, NULL);
    if (IS_STATIC_INIT(m->real_ptr)) {
        uintptr_t v = (uintptr_t)m->real_ptr;
        int kind = PTHREAD_MUTEX_NORMAL;
        if (v == BIONIC_PTHREAD_RECURSIVE_MUTEX_INITIALIZER) kind = PTHREAD_MUTEX_RECURSIVE;
        else if (v == BIONIC_PTHREAD_ERRORCHECK_MUTEX_INITIALIZER) kind = PTHREAD_MUTEX_ERRORCHECK;
        mutex_alloc(m, kind);
    }
    sceKernelUnlockLwMutex(lk, 1);
    return IS_STATIC_INIT(m->real_ptr) ? NULL : m->real_ptr;
}

int pthread_mutex_init_soloader(pthread_mutex_t_bionic *m, const pthread_mutexattr_t *attr) {
    if (!m) return EINVAL;
    int kind = PTHREAD_MUTEX_NORMAL;
    if (attr) pthread_mutexattr_gettype((pthread_mutexattr_t *)attr, &kind);
    m->real_ptr = NULL;
    return mutex_alloc(m, kind);
}

int pthread_mutex_destroy_soloader(pthread_mutex_t_bionic *m) {
    if (!m) return 0;
    if (IS_STATIC_INIT(m->real_ptr)) return 0;
    int ret = pthread_mutex_destroy(m->real_ptr);
    free(m->real_ptr);
    m->real_ptr = NULL;
    return ret;
}

int pthread_mutex_lock_soloader(pthread_mutex_t_bionic *m) {
    if (!m) return EINVAL;
    pthread_mutex_t *r = mutex_get(m);
    return r ? pthread_mutex_lock(r) : EINVAL;
}

int pthread_mutex_trylock_soloader(pthread_mutex_t_bionic *m) {
    if (!m) return EINVAL;
    pthread_mutex_t *r = mutex_get(m);
    return r ? pthread_mutex_trylock(r) : EINVAL;
}

int pthread_mutex_unlock_soloader(pthread_mutex_t_bionic *m) {
    if (!m) return EINVAL;
    if (IS_STATIC_INIT(m->real_ptr)) return EPERM;
    return pthread_mutex_unlock(m->real_ptr);
}

/* ------------------------------------------------------------------------------------------ */
/* condition variables                                                                         */
/* ------------------------------------------------------------------------------------------ */

int pthread_condattr_init_soloader(pthread_condattr_t *attr) { return attr ? pthread_condattr_init(attr) : EINVAL; }
int pthread_condattr_destroy_soloader(pthread_condattr_t *attr) { return attr ? pthread_condattr_destroy(attr) : EINVAL; }

static pthread_cond_t *cond_get(pthread_cond_t_bionic *c) {
    if (!IS_STATIC_INIT(c->real_ptr)) return c->real_ptr;

    SceKernelLwMutexWork *lk = get_init_lock();
    sceKernelLockLwMutex(lk, 1, NULL);
    if (IS_STATIC_INIT(c->real_ptr)) {
        pthread_cond_t *real = malloc(sizeof(pthread_cond_t));
        if (real && pthread_cond_init(real, NULL) == 0) c->real_ptr = real;
        else { free(real); l_error("cond init for %p failed", c); }
    }
    sceKernelUnlockLwMutex(lk, 1);
    return IS_STATIC_INIT(c->real_ptr) ? NULL : c->real_ptr;
}

int pthread_cond_init_soloader(pthread_cond_t_bionic *c, const pthread_condattr_t *attr) {
    if (!c) return EINVAL;
    pthread_cond_t *real = malloc(sizeof(pthread_cond_t));
    if (!real) return ENOMEM;
    int ret = pthread_cond_init(real, attr);
    if (ret != 0) { free(real); return ret; }
    c->real_ptr = real;
    return 0;
}

int pthread_cond_destroy_soloader(pthread_cond_t_bionic *c) {
    if (!c) return 0;
    if (IS_STATIC_INIT(c->real_ptr)) return 0;
    int ret = pthread_cond_destroy(c->real_ptr);
    free(c->real_ptr);
    c->real_ptr = NULL;
    return ret;
}

int pthread_cond_signal_soloader(pthread_cond_t_bionic *c) {
    if (!c) return EINVAL;
    pthread_cond_t *r = cond_get(c);
    return r ? pthread_cond_signal(r) : EINVAL;
}

int pthread_cond_broadcast_soloader(pthread_cond_t_bionic *c) {
    if (!c) return EINVAL;
    pthread_cond_t *r = cond_get(c);
    return r ? pthread_cond_broadcast(r) : EINVAL;
}

int pthread_cond_wait_soloader(pthread_cond_t_bionic *c, pthread_mutex_t_bionic *m) {
    if (!c || !m) return EINVAL;
    pthread_cond_t *rc = cond_get(c);
    pthread_mutex_t *rm = mutex_get(m);
    return (rc && rm) ? pthread_cond_wait(rc, rm) : EINVAL;
}

int pthread_cond_timedwait_soloader(pthread_cond_t_bionic *c, pthread_mutex_t_bionic *m, const struct timespec *abstime) {
    if (!c || !m || !abstime) return EINVAL;
    pthread_cond_t *rc = cond_get(c);
    pthread_mutex_t *rm = mutex_get(m);
    return (rc && rm) ? pthread_cond_timedwait(rc, rm, abstime) : EINVAL;
}

/* abstime is on CLOCK_MONOTONIC: convert to a CLOCK_REALTIME deadline */
int pthread_cond_timedwait_monotonic_np_soloader(pthread_cond_t_bionic *c, pthread_mutex_t_bionic *m, const struct timespec *abstime) {
    if (!c || !m || !abstime) return EINVAL;
    struct timespec mono, real, abs;
    mono.tv_sec = sceKernelGetProcessTimeWide() / 1000000; mono.tv_nsec = (sceKernelGetProcessTimeWide() % 1000000) * 1000;
    clock_gettime(CLOCK_REALTIME, &real);
    int64_t rel_ns = ((int64_t)abstime->tv_sec - mono.tv_sec) * 1000000000LL + ((int64_t)abstime->tv_nsec - mono.tv_nsec);
    if (rel_ns < 0) rel_ns = 0;
    int64_t t = (int64_t)real.tv_sec * 1000000000LL + real.tv_nsec + rel_ns;
    abs.tv_sec = t / 1000000000LL;
    abs.tv_nsec = t % 1000000000LL;
    return pthread_cond_timedwait_soloader(c, m, &abs);
}

/* semaphores (bionic sem_t is a 32-bit word holding the SceUID): reimpl/vsig.c (interruptible waits) */

/* ------------------------------------------------------------------------------------------ */
/* __pthread_cleanup_push/pop: struct __pthread_cleanup_t { prev; routine; arg } (bionic ABI) */
/* ------------------------------------------------------------------------------------------ */

typedef struct pthread_cleanup_bionic {
    struct pthread_cleanup_bionic *prev;
    void (*routine)(void *);
    void *arg;
} pthread_cleanup_bionic;

void pthread_cleanup_push_soloader(void *c, void (*routine)(void *), void *arg) {
    pthread_cleanup_bionic *cl = c;
    cl->prev = NULL;       /* we never unwind on pthread_exit: only the explicit pop(execute) runs it */
    cl->routine = routine;
    cl->arg = arg;
}

void pthread_cleanup_pop_soloader(void *c, int execute) {
    pthread_cleanup_bionic *cl = c;
    if (execute && cl->routine) cl->routine(cl->arg);
}
