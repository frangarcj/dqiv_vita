/*
 * jni_impl.c -- FalsoJNI tables for the native port.
 *
 * libdq4core's and libsdlib's JNI_OnLoad(vm, NULL) never call into the VM (dq4core stores a flag that
 * IsLoadNativeCode() returns, sdlib stores the VM pointer), and the single Java-facing entry we call
 * (Java_..._LoadFileList) only forwards its AssetManager argument to AAssetManager_fromJava. So no Java
 * method / field is needed: empty tables.  Add entries here if a later feature (dialogs, locale) needs one.
 */
#include <falso_jni/FalsoJNI.h>
#include <falso_jni/FalsoJNI_Impl.h>

NameToMethodID nameToMethodId[] = {};

MethodsBoolean methodsBoolean[] = {};
MethodsByte methodsByte[] = {};
MethodsChar methodsChar[] = {};
MethodsDouble methodsDouble[] = {};
MethodsFloat methodsFloat[] = {};
MethodsInt methodsInt[] = {};
MethodsLong methodsLong[] = {};
MethodsObject methodsObject[] = {};
MethodsShort methodsShort[] = {};
MethodsVoid methodsVoid[] = {};

NameToFieldID nameToFieldId[] = {};

FieldsBoolean fieldsBoolean[] = {};
FieldsByte fieldsByte[] = {};
FieldsChar fieldsChar[] = {};
FieldsDouble fieldsDouble[] = {};
FieldsFloat fieldsFloat[] = {};
FieldsInt fieldsInt[] = {};
FieldsObject fieldsObject[] = {};
FieldsLong fieldsLong[] = {};
FieldsShort fieldsShort[] = {};

__FALSOJNI_IMPL_CONTAINER_SIZES
