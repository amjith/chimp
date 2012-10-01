#ifndef _CHIMP_OBJECT_H_INCLUDED_
#define _CHIMP_OBJECT_H_INCLUDED_

#include <chimp/core.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum _ChimpValueType {
    CHIMP_VALUE_TYPE_ANY,
    CHIMP_VALUE_TYPE_OBJECT,
    CHIMP_VALUE_TYPE_CLASS,
    CHIMP_VALUE_TYPE_STR,
} ChimpValueType;

typedef struct _ChimpAny {
    ChimpValueType  type;
    ChimpRef       *klass;
} ChimpAny;

typedef enum _ChimpCmpResult {
    CHIMP_CMP_ERROR    = -3,
    CHIMP_CMP_NOT_IMPL = -2,
    CHIMP_CMP_LT       = -1,
    CHIMP_CMP_EQ       = 0,
    CHIMP_CMP_GT       = 1
} ChimpCmpResult;

typedef struct _ChimpClass {
    ChimpAny  base;
    ChimpRef *name;
    ChimpRef *super;
    ChimpCmpResult (*cmp)(ChimpRef *, ChimpRef *);
} ChimpClass;

typedef struct _ChimpObject {
    ChimpAny  base;
} ChimpObject;

typedef struct _ChimpStr {
    ChimpAny  base;
    char     *data;
    size_t    size;
} ChimpStr;

typedef union _ChimpValue {
    ChimpAny    any;
    ChimpClass  klass;
    ChimpObject object;
    ChimpStr    str;
} ChimpValue;

#define CHIMP_CHECK_CAST(struc, ref, type) ((struc *) chimp_gc_ref_check_cast ((ref), (type)))

#define CHIMP_ANY(ref)    CHIMP_CHECK_CAST(ChimpAny, (ref), CHIMP_VALUE_TYPE_ANY)
#define CHIMP_CLASS(ref)  CHIMP_CHECK_CAST(ChimpClass, (ref), CHIMP_VALUE_TYPE_CLASS)
#define CHIMP_OBJECT(ref) CHIMP_CHECK_CAST(ChimpObject, (ref), CHIMP_VALUE_TYPE_OBJECT)
#define CHIMP_STR(ref)    CHIMP_CHECK_CAST(ChimpStr, (ref), CHIMP_VALUE_TYPE_STR)

#ifdef __cplusplus
};
#endif

#endif

