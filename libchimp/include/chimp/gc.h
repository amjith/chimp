#ifndef _CHIMP_GC_H_INCLUDED_
#define _CHIMP_GC_H_INCLUDED_

#include <chimp/core.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _ChimpGC ChimpGC;
typedef struct _ChimpRef ChimpRef;

union _ChimpValue;
enum _ChimpValueType;

ChimpGC *
chimp_gc_new (void *stack_start);

void
chimp_gc_delete (ChimpGC *gc);

ChimpRef *
chimp_gc_new_object (ChimpGC *gc);

chimp_bool_t
chimp_gc_make_root (ChimpGC *gc, ChimpRef *ref);

union _ChimpValue *
chimp_gc_ref_check_cast (ChimpRef *ref, enum _ChimpValueType type);

enum _ChimpValueType
chimp_gc_ref_type (ChimpRef *ref);

chimp_bool_t
chimp_gc_collect (ChimpGC *gc);

void
chimp_gc_mark_ref (ChimpGC *gc, ChimpRef *ref);

uint64_t
chimp_gc_collection_count (ChimpGC *gc);

uint64_t
chimp_gc_num_live (ChimpGC *gc);

uint64_t
chimp_gc_num_free (ChimpGC *gc);

#define CHIMP_REF_TYPE(ref) chimp_gc_ref_type(ref)

#define CHIMP_GC_MAKE_STACK_ROOT(p) \
    (*((ChimpRef **)alloca(sizeof(ChimpRef *)))) = (p)

#ifdef __cplusplus
};
#endif

#endif

