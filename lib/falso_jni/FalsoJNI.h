/*
 * FalsoJNI.h
 *
 * Fake Java Native Interface, providing JavaVM and JNIEnv objects.
 *
 * Copyright (C) 2022 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#ifndef FALSOJNI_H
#define FALSOJNI_H

#define FALSOJNI_DEBUG_NO    4
#define FALSOJNI_DEBUG_ERROR 3
#define FALSOJNI_DEBUG_WARN  2
#define FALSOJNI_DEBUG_INFO  1
#define FALSOJNI_DEBUG_ALL   0

#ifndef FALSOJNI_DEBUGLEVEL
#define FALSOJNI_DEBUGLEVEL FALSOJNI_DEBUG_WARN
#endif

#include "jni.h"

#ifdef FALSOJNI_IMPLEMENTATION_SAMPLE
#include "FalsoJNI_ImplSample.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern JavaVM jvm;
extern JNIEnv jni;

void jni_init();

/* dqiv: natives registered through RegisterNatives */
#define FJNI_MAX_NATIVES 512
typedef struct { char *cls; char *name; char *sig; void *fn; } fjni_native;
extern fjni_native fjni_natives[FJNI_MAX_NATIVES];
extern int fjni_natives_count;
void *fjni_find_native(const char *name);

#ifdef __cplusplus
}
#endif

#endif // FALSOJNI_H
