/*****************************************************************************
 *                                                                           *
 * Copyright 2012 Thomas Lee                                                 *
 *                                                                           *
 * Licensed under the Apache License, Version 2.0 (the "License");           *
 * you may not use this file except in compliance with the License.          *
 * You may obtain a copy of the License at                                   *
 *                                                                           *
 *     http://www.apache.org/licenses/LICENSE-2.0                            *
 *                                                                           *
 * Unless required by applicable law or agreed to in writing, software       *
 * distributed under the License is distributed on an "AS IS" BASIS,         *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  *
 * See the License for the specific language governing permissions and       *
 * limitations under the License.                                            *
 *                                                                           *
 *****************************************************************************/

#ifdef HAVE_VALGRIND
#include <valgrind/memcheck.h>
#else
#define VALGRIND_MAKE_MEM_DEFINED(p, s)
#endif

#include "chimp/gc.h"
#include "chimp/object.h"
#include "chimp/core.h"
#include "chimp/task.h"
#include "chimp/_parser.h"

/* 64k default heap (CHIMP_VALUE_SIZE * DEFAULT_SLAB_SIZE) */
#define DEFAULT_SLAB_SIZE 256

struct _ChimpRef {
    chimp_bool_t marked;
    struct _ChimpRef *next;
    char value[CHIMP_VALUE_SIZE];
};

#define CHIMP_FAST_ANY(ref) ((ChimpAny *)(ref)->value)

typedef struct _ChimpSlab {
    ChimpRef *refs;
    void *head;
} ChimpSlab;

typedef struct _ChimpHeap {
    ChimpSlab **slabs;
    size_t      slab_count;
    size_t      slab_size;
    size_t      used;
} ChimpHeap;

struct _ChimpGC {
    ChimpHeap  heap;

    ChimpRef  *live;
    ChimpRef  *free;

    ChimpRef **roots;
    size_t     num_roots;

    void      *stack_start;
    uint64_t   collection_count;
};

#define CHIMP_HEAP_CURRENT_SLAB(heap) \
    (heap)->slabs[(heap)->slab_count - 1]

#define CHIMP_HEAP_ALLOCATED(heap) \
    ((heap)->slab_count * ((heap)->slab_size * sizeof(ChimpRef)))

static ChimpSlab *
chimp_slab_new (size_t size)
{
    ChimpRef *refs;
    size_t i;
    ChimpSlab *slab =
      CHIMP_MALLOC (ChimpSlab, sizeof (*slab) + sizeof(ChimpRef) * size);
    if (slab == NULL) {
        return NULL;
    }
    refs = (ChimpRef *)(((char *) slab) + sizeof (*slab));
    slab->head = refs;
    slab->refs = refs;
    /* TODO don't think we need the conditional check */
    if (size > 1) {
        for (i = 1; i < size; i++) {
            refs[i-1].next = refs + i;
        }
        refs[size-1].next = NULL;
    }
    else if (size == 1) {
        refs->next = NULL;
    }
    return slab;
}

static chimp_bool_t
chimp_heap_init (ChimpHeap *heap, size_t slab_size)
{
    heap->slabs = CHIMP_MALLOC (ChimpSlab *, sizeof (*heap->slabs));
    if (heap->slabs == NULL) {
        return CHIMP_FALSE;
    }
    heap->slabs[0] = chimp_slab_new (slab_size);
    if (heap->slabs[0] == NULL) {
        CHIMP_FREE (heap->slabs);
        return CHIMP_FALSE;
    }
    heap->slab_count = 1;
    heap->slab_size  = slab_size;
    heap->used       = 0;
    return CHIMP_TRUE;
}

static void
chimp_heap_destroy (ChimpHeap *heap)
{
    if (heap != NULL) {
        size_t i;
        for (i = 0; i < heap->slab_count; i++) {
            CHIMP_FREE (heap->slabs[i]);
        }
        CHIMP_FREE(heap->slabs);
    }
}

static chimp_bool_t
chimp_heap_grow (ChimpHeap *heap)
{
    ChimpSlab **slabs;
    ChimpSlab *slab;

    slab = chimp_slab_new (heap->slab_size);
    if (slab == NULL) {
        return CHIMP_FALSE;
    }
    slabs = CHIMP_REALLOC (
        ChimpSlab *, heap->slabs,
        sizeof (*heap->slabs) * (heap->slab_count + 1));
    if (slabs == NULL) {
        CHIMP_FREE (slab);
        return CHIMP_FALSE;
    }
    heap->slabs = slabs;
    slabs[heap->slab_count++] = slab;
    return CHIMP_TRUE;
}

static chimp_bool_t
chimp_heap_contains (ChimpHeap *heap, void *value)
{
    size_t i;
    for (i = 0; i < heap->slab_count; i++) {
        void *begin = heap->slabs[i]->head;
        void *end   = heap->slabs[i]->head + sizeof(ChimpRef) * heap->slab_size;
        
        if (value >= begin && value < end) {
            /* is this a pointer to the *start* of a value/ref?
             * (We don't want to corrupt random bytes in the heap during a mark)
             */
            if ((end - value) % sizeof(ChimpRef) == 0) {
                return CHIMP_TRUE;
            }
        }
    }
    return CHIMP_FALSE;
}

ChimpGC *
chimp_gc_new (void *stack_start)
{
    ChimpGC *gc = CHIMP_MALLOC (ChimpGC, sizeof (*gc));
    if (gc == NULL) {
        return NULL;
    }

    memset (gc, 0, sizeof (*gc));

    gc->stack_start = stack_start;

    if (!chimp_heap_init (&gc->heap, DEFAULT_SLAB_SIZE)) {
        CHIMP_FREE (gc);
        return NULL;
    }
    gc->free = gc->heap.slabs[0]->head;
    return gc;
}

static void
chimp_gc_value_dtor (ChimpGC *gc, ChimpRef *ref)
{
    if (!chimp_heap_contains (&gc->heap, ref)) {
        CHIMP_BUG ("destructor called on value that belongs to another GC: %s",
                CHIMP_STR(chimp_object_str (ref))->data);
        return;
    }

    if (CHIMP_CLASS(CHIMP_ANY_CLASS(ref))->dtor) {
        CHIMP_CLASS(CHIMP_ANY_CLASS(ref))->dtor (ref);
    }
}

void
chimp_gc_delete (ChimpGC *gc)
{
    if (gc != NULL) {
        ChimpRef *live = gc->live;
        while (live != NULL) {
            ChimpRef *next = live->next;
            chimp_gc_value_dtor (gc, live);
            live = next;
        }
        gc->live = NULL;

        chimp_heap_destroy (&gc->heap);
        CHIMP_FREE (gc->roots);
        CHIMP_FREE (gc);
    }
}

ChimpRef *
chimp_gc_new_object (ChimpGC *gc)
{
    ChimpRef *ref;
    
    if (gc == NULL) {
        gc = CHIMP_CURRENT_GC;
    }

    if (gc->free == NULL) {
        if (!chimp_gc_collect (gc)) {
            ChimpSlab *slab;
            if (!chimp_heap_grow (&gc->heap)) {
                CHIMP_BUG ("out of memory");
                return NULL;
            }
            slab = CHIMP_HEAP_CURRENT_SLAB(&gc->heap);
            gc->free = slab->head;
        }
    }

    ref = gc->free;
    gc->free = gc->free->next;

    memset (ref, 0, sizeof(*ref));
    ref->next = gc->live;
    gc->live = ref;
    gc->heap.used++;
    return ref;
}

chimp_bool_t
chimp_gc_make_root (ChimpGC *gc, ChimpRef *ref)
{
    ChimpRef **roots;

    if (gc == NULL) {
        gc = CHIMP_CURRENT_GC;
    }
    
    roots = CHIMP_REALLOC (ChimpRef *, gc->roots, sizeof (*gc->roots) * (gc->num_roots + 1));
    if (roots == NULL) {
        return CHIMP_FALSE;
    }
    gc->roots = roots;
    gc->roots[gc->num_roots++] = ref;
    return CHIMP_TRUE;
}

void *
chimp_gc_ref_check_cast (ChimpRef *ref, ChimpRef *klass)
{
    if (ref == NULL) {
        if (klass != NULL) {
            CHIMP_BUG ("expected ref type '%s', but got a null ref",
                        CHIMP_STR_DATA(CHIMP_CLASS (klass)->name));
        }
        else {
            CHIMP_BUG ("expected non-null ref");
        }
        return NULL;
    }

    if (klass == NULL) {
        return ref->value;
    }

    /* TODO need an 'instanceof' type check here */
    if (CHIMP_ANY_CLASS(ref) != klass) {
        CHIMP_BUG ("expected ref type '%s', but got '%s'",
                    CHIMP_STR_DATA(CHIMP_CLASS (klass)->name),
                    CHIMP_STR_DATA(CHIMP_CLASS (CHIMP_ANY_CLASS(ref))->name));
        return NULL;
    }

    return ref->value;
}

void
chimp_gc_mark_ref (ChimpGC *gc, ChimpRef *ref)
{
    ChimpRef *klass;

    if (ref == NULL) return;

    if (gc == NULL) {
        gc = CHIMP_CURRENT_GC;
    }

    if (!chimp_heap_contains (&gc->heap, ref)) {
        /* this ref belongs to another GC */
        return;
    }

    if (ref->marked) return;
    ref->marked = CHIMP_TRUE;

    chimp_gc_mark_ref (gc, CHIMP_FAST_ANY(ref)->klass);
    klass = CHIMP_FAST_ANY(ref)->klass;

    if (CHIMP_CLASS(klass)->mark) {
        CHIMP_CLASS(klass)->mark (gc, ref);
    }
    else {
        CHIMP_BUG ("No mark for class %s", CHIMP_STR_DATA(CHIMP_CLASS(klass)->name));
    }
}

static void
_chimp_gc_mark_lwhash_item (
    ChimpLWHash *self, ChimpRef *key, ChimpRef *value, void *data)
{
    ChimpGC *gc = (ChimpGC *) data;
    chimp_gc_mark_ref (gc, key);
    chimp_gc_mark_ref (gc, value);
}

void
chimp_gc_mark_lwhash (ChimpGC *gc, ChimpLWHash *lwhash)
{
    chimp_lwhash_foreach (lwhash, _chimp_gc_mark_lwhash_item, gc);
}

static size_t
chimp_gc_sweep (ChimpGC *gc)
{
    ChimpRef *ref = gc->live;
    ChimpRef *live = NULL;
    ChimpRef *free_ = NULL;
    size_t kept = 0;
    size_t freed = 0;

    while (ref != NULL) {
        ChimpRef *next = ref->next;
        if (!chimp_heap_contains (&gc->heap, ref)) {
            /* this ref belongs to another GC*/
            ref = next;
            continue;
        }
        if (ref->marked) {
            ref->next = live;
            live = ref;
            kept++;
        }
        else {
            if (CHIMP_ANY_CLASS (ref) == chimp_class_class) {
                CHIMP_BUG ("collected class: %s", CHIMP_STR_DATA (CHIMP_CLASS (ref)->name));
            }
            chimp_gc_value_dtor (gc, ref);
            ref->next = free_;
            free_ = ref;
            freed++;
        }
        ref = next;
    }

    gc->live = live;
    gc->free = free_;
    gc->heap.used -= freed;
    return freed;
}

#if (defined CHIMP_ARCH_X86_64) && (defined __GNUC__)
#define CHIMP_GC_GET_STACK_END(ptr, guess) \
    __asm__("movq %%rsp, %0" : "=r" (ptr))
#elif (defined CHIMP_ARCH_X86_32) && (defined __GNUC__)
#define CHIMP_GC_GET_STACK_END(ptr, guess) \
    __asm__("movq %%esp, %0" : "=r" (ptr))
#else
#warning "Unknown or unsupported architecture: GC must guess at stack end"
#define CHIMP_GC_GET_STACK_END(ptr, guess) (ptr) = (guess)
#endif

chimp_bool_t
chimp_gc_collect (ChimpGC *gc)
{
    size_t i;
    ChimpRef *ref;
    ChimpRef *base;
    /* save registers to the stack */
#if (defined CHIMP_ARCH_X86_64) && (defined __GNUC__)
    void *regs[16];
    __asm__ volatile ("push %rax;");
    __asm__ volatile (
         "movq %%rbx, 8(%%rax);"
         "movq %%rcx, 16(%%rax);"
         "movq %%rdx, 24(%%rax);"
         "movq %%rsi, 32(%%rax);"
         "movq %%rdi, 40(%%rax);"
         "movq %%r8,  48(%%rax);"
         "movq %%r9,  56(%%rax);"
         "movq %%r10, 64(%%rax);"
         "movq %%r11, 72(%%rax);"
         "movq %%r12, 80(%%rax);"
         "movq %%r13, 88(%%rax);"
         "movq %%r14, 96(%%rax);"
         "movq %%r15, 104(%%rax);"
         "movq %%rbp, 112(%%rax);"
         "movq %%rsp, 120(%%rax);"
         "pop %%rbx;"
         "movq %%rbx, 128(%%rax);"
         :
         : "a" (regs)
         : "%rbx", "memory");
#elif (defined CHIMP_ARCH_X86_32) && (defined __GNUC__)
    /* XXX untested */
    void *regs[8];
    __asm__ volatile ("push %eax;");
    __asm__ volatile (
        "movl %%ebx, 4(%%eax);"
        "movl %%ecx, 8(%%eax);"
        "movl %%edx, 12(%%eax);"
        "movl %%esi, 16(%%eax);"
        "movl %%edi, 20(%%eax);"
        "movl %%ebp, 24(%%eax);"
        "movl %%esp, 28(%%eax);"
        "pop %%ebx;"
        "movl %%ebx, 32(%%eax);"
        :
        : "a" (regs)
        : "%ebx", "memory");
#else
#warning "Unknown or unsupported architecture: GC can't grok registers"
#endif
    
    base = NULL;
    base = base;

    if (gc == NULL) {
        gc = CHIMP_CURRENT_GC;
    }

    gc->collection_count++;

    ref = gc->live;
    while (ref != NULL) {
        ref->marked = CHIMP_FALSE;
        ref = ref->next;
    }

    for (i = 0; i < gc->num_roots; i++) {
        chimp_gc_mark_ref (gc, gc->roots[i]);
    }

    chimp_task_mark (gc, CHIMP_CURRENT_TASK);

    if (gc->stack_start != NULL) {
        void *ref_p;
        
        CHIMP_GC_GET_STACK_END(ref_p, &base);

        /* XXX stack may grow in the other direction on some archs. */
        while (ref_p <= gc->stack_start) {
            /* STFU valgrind. */
            VALGRIND_MAKE_MEM_DEFINED(ref_p, sizeof(ChimpRef *));

            if (chimp_heap_contains (&gc->heap, *((ChimpRef **)ref_p))) {
                chimp_gc_mark_ref (gc, *((ChimpRef **)ref_p));
            }
            ref_p += sizeof(ref_p);
        }
    }

    return chimp_gc_sweep (gc) > 0;
}

uint64_t
chimp_gc_collection_count (ChimpGC *gc)
{
    if (gc == NULL) {
        gc = CHIMP_CURRENT_GC;
    }

    return gc->collection_count;
}

uint64_t
chimp_gc_num_live (ChimpGC *gc)
{
    if (gc == NULL) {
        gc = CHIMP_CURRENT_GC;
    }

    return gc->heap.used;
}

uint64_t
chimp_gc_num_free (ChimpGC *gc)
{
    if (gc == NULL) {
        gc = CHIMP_CURRENT_GC;
    }

    return (gc->heap.slab_count * gc->heap.slab_size) - gc->heap.used;
}

