#include "chimp/core.h"
#include "chimp/gc.h"
#include "chimp/object.h"
#include "chimp/lwhash.h"
#include "chimp/class.h"
#include "chimp/str.h"
#include "chimp/int.h"
#include "chimp/array.h"
#include "chimp/method.h"
#include "chimp/task.h"
#include "chimp/hash.h"
#include "chimp/frame.h"
#include "chimp/ast.h"
#include "chimp/code.h"
#include "chimp/module.h"

#define CHIMP_BOOTSTRAP_CLASS_L1(gc, c, n, sup) \
    do { \
        CHIMP_ANY(c)->type = CHIMP_VALUE_TYPE_CLASS; \
        CHIMP_ANY(c)->klass = chimp_class_class; \
        CHIMP_CLASS(c)->super = (sup); \
        CHIMP_CLASS(c)->name = chimp_gc_new_object ((gc)); \
        CHIMP_ANY(CHIMP_CLASS(c)->name)->type = CHIMP_VALUE_TYPE_STR; \
        CHIMP_ANY(CHIMP_CLASS(c)->name)->klass = chimp_str_class; \
        CHIMP_STR(CHIMP_CLASS(c)->name)->data = strndup ((n), (sizeof(n)-1)); \
        if (CHIMP_STR(CHIMP_CLASS(c)->name)->data == NULL) { \
            chimp_task_delete (main_task); \
            main_task = NULL; \
            return CHIMP_FALSE; \
        } \
        CHIMP_STR(CHIMP_CLASS(c)->name)->size = sizeof(n)-1; \
        chimp_gc_make_root ((gc), (c)); \
    } while (0)

#define CHIMP_BOOTSTRAP_CLASS_L2(gc, c) \
    do { \
    } while (0)

ChimpRef *chimp_object_class = NULL;
ChimpRef *chimp_class_class = NULL;
ChimpRef *chimp_str_class = NULL;
ChimpRef *chimp_nil_class = NULL;
ChimpRef *chimp_bool_class = NULL;

ChimpRef *chimp_nil = NULL;
ChimpRef *chimp_true = NULL;
ChimpRef *chimp_false = NULL;
ChimpRef *chimp_builtins = NULL;

static ChimpCmpResult
chimp_str_cmp (ChimpRef *a, ChimpRef *b)
{
    ChimpStr *as;
    ChimpStr *bs;
    ChimpValueType at, bt;

    if (a == b) {
        return CHIMP_CMP_EQ;
    }

    at = CHIMP_REF_TYPE(a);
    bt = CHIMP_REF_TYPE(b);

    if (at != CHIMP_VALUE_TYPE_STR) {
        /* TODO this is almost certainly a bug */
        return CHIMP_CMP_ERROR;
    }

    /* TODO should probably be a subtype check? */
    if (at != bt) {
        return CHIMP_CMP_NOT_IMPL;
    }

    as = CHIMP_STR(a);
    bs = CHIMP_STR(b);

    if (as->size != bs->size) {
        int r = strcmp (as->data, bs->data);
        if (r < 0) {
            return CHIMP_CMP_LT;
        }
        else if (r > 0) {
            return CHIMP_CMP_GT;
        }
        else {
            return CHIMP_CMP_EQ;
        }
    }

    return memcmp (CHIMP_STR(a)->data, CHIMP_STR(b)->data, as->size);
}

static ChimpRef *
_chimp_object_str (ChimpGC *gc, ChimpRef *self)
{
    char buf[32];
    ChimpRef *name = CHIMP_CLASS_NAME(CHIMP_ANY_CLASS(self));
    ChimpRef *str;
    
    snprintf (buf, sizeof(buf), " @ %p>", self);
    str = chimp_str_new_concat (gc, "<", CHIMP_STR_DATA(name), buf, NULL);
    if (str == NULL) {
        return NULL;
    }
    return str;
}

static ChimpRef *
chimp_bool_str (ChimpGC *gc, ChimpRef *self)
{
    if (self == chimp_true) {
        return CHIMP_STR_NEW(gc, "true");
    }
    else if (self == chimp_false) {
        return CHIMP_STR_NEW(gc, "false");
    }
    else {
        return NULL;
    }
}

static ChimpRef *
chimp_str_str (ChimpGC *gc, ChimpRef *self)
{
    return self;
}

static ChimpRef *
_chimp_object_getattr (ChimpRef *self, ChimpRef *name)
{
    /* TODO check CHIMP_ANY(self)->attributes ? */
    ChimpLWHash *methods = CHIMP_CLASS(CHIMP_ANY_CLASS(self))->methods;
    ChimpRef *method;
    if (!chimp_lwhash_get (methods, name, &method)) {
        return NULL;
    }
    if (method == NULL) {
        return NULL;
    }
    /* XXX binding on every call is probably dumb/slow */
    return chimp_method_new_bound (method, self);
}

static ChimpRef *
chimp_class_getattr (ChimpRef *self, ChimpRef *name)
{
    ChimpLWHash *methods = CHIMP_CLASS(self)->methods;
    ChimpRef *method;
    if (!chimp_lwhash_get (methods, name, &method)) {
        return NULL;
    }
    if (method == NULL) {
        return NULL;
    }
    return method;
}

static ChimpRef *
chimp_nil_str (ChimpGC *gc, ChimpRef *self)
{
    return CHIMP_CLASS_NAME(CHIMP_ANY_CLASS(self));
}

static ChimpTask *main_task = NULL;

static ChimpRef *
_print (ChimpRef *self, ChimpRef *args)
{
    size_t i;

    for (i = 0; i < CHIMP_ARRAY_SIZE(args); i++) {
        ChimpRef *str = chimp_object_str (NULL, CHIMP_ARRAY_ITEM (args, i));
        printf ("%s\n", CHIMP_STR_DATA(str));
    }

    return chimp_nil;
}

static ChimpRef *
_input (ChimpRef *self, ChimpRef *args)
{
    char buf[1024];
    size_t len;

    if (fgets (buf, sizeof(buf), stdin) == NULL) {
        return chimp_nil;
    }

    len = strlen(buf);
    if (len > 0 && buf[len-1] == '\n') {
        buf[--len] = '\0';
    }
    return chimp_str_new (NULL, buf, len);
}

static ChimpRef *
chimp_core_init_io_module (void)
{
    ChimpRef *io;
    ChimpRef *exports;
    ChimpRef *print_method;
    ChimpRef *input_method;

    print_method = chimp_method_new_native (NULL, NULL, _print);
    input_method = chimp_method_new_native (NULL, NULL, _input);
    exports = chimp_hash_new (NULL);
    chimp_hash_put_str (exports, "print", print_method);
    chimp_hash_put_str (exports, "readline", input_method);
    
    io = chimp_module_new_str ("io", exports);
    if (io == NULL) {
        return NULL;
    }

    /* XXX stupid hack because I can't think far enough ahead of myself */
    CHIMP_METHOD(print_method)->module = io;
    CHIMP_METHOD(input_method)->module = io;

    return io;
}

static chimp_bool_t
chimp_core_init_builtins (void)
{
    chimp_builtins = chimp_hash_new (NULL);
    if (chimp_builtins == NULL) {
        return CHIMP_FALSE;
    }
    chimp_gc_make_root (NULL, chimp_builtins);

    /* TODO error checking */

    /* XXX we can't call methods on the i/o module yet (we don't have the
     *     syntax for it), so use these instead.
     */
    chimp_hash_put_str (chimp_builtins, "print",  chimp_method_new_native (NULL, NULL, _print));
    chimp_hash_put_str (chimp_builtins, "input",  chimp_method_new_native (NULL, NULL, _input));

    chimp_hash_put_str (chimp_builtins, "hash",   chimp_hash_class);
    chimp_hash_put_str (chimp_builtins, "array",  chimp_array_class);
    chimp_hash_put_str (chimp_builtins, "int",    chimp_int_class);
    chimp_hash_put_str (chimp_builtins, "str",    chimp_str_class);
    chimp_hash_put_str (chimp_builtins, "module", chimp_module_class);
    chimp_hash_put_str (chimp_builtins, "object", chimp_object_class);
    chimp_hash_put_str (chimp_builtins, "class",  chimp_class_class);
    chimp_hash_put_str (chimp_builtins, "method", chimp_method_class);

    chimp_task_add_module (NULL, chimp_core_init_io_module ());

    return CHIMP_TRUE;
}

chimp_bool_t
chimp_core_startup (void *stack_start)
{
    main_task = chimp_task_new_main (stack_start);
    if (main_task == NULL) {
        return CHIMP_FALSE;
    }

    chimp_object_class = chimp_gc_new_object (NULL);
    chimp_class_class  = chimp_gc_new_object (NULL);
    chimp_str_class    = chimp_gc_new_object (NULL);

    CHIMP_BOOTSTRAP_CLASS_L1(NULL, chimp_object_class, "object", NULL);
    CHIMP_CLASS(chimp_object_class)->str = _chimp_object_str;
    CHIMP_CLASS(chimp_object_class)->getattr = _chimp_object_getattr;
    CHIMP_BOOTSTRAP_CLASS_L1(NULL, chimp_class_class, "class", chimp_object_class);
    CHIMP_CLASS(chimp_class_class)->getattr = chimp_class_getattr;
    CHIMP_BOOTSTRAP_CLASS_L1(NULL, chimp_str_class, "str", chimp_object_class);
    CHIMP_CLASS(chimp_str_class)->cmp = chimp_str_cmp;
    CHIMP_CLASS(chimp_str_class)->str = chimp_str_str;

    CHIMP_BOOTSTRAP_CLASS_L2(NULL, chimp_object_class);
    CHIMP_BOOTSTRAP_CLASS_L2(NULL, chimp_class_class);
    CHIMP_BOOTSTRAP_CLASS_L2(NULL, chimp_str_class);

    if (!_chimp_bootstrap_L3 ()) {
        return CHIMP_FALSE;
    }

    chimp_nil_class = chimp_class_new (NULL, CHIMP_STR_NEW(NULL, "nil"), chimp_object_class);
    if (chimp_nil_class == NULL) goto error;
    chimp_gc_make_root (NULL, chimp_nil_class);
    CHIMP_CLASS(chimp_nil_class)->str = chimp_nil_str;
    chimp_nil = chimp_object_new (NULL, chimp_nil_class);
    if (chimp_nil == NULL) goto error;
    chimp_gc_make_root (NULL, chimp_nil);

    chimp_bool_class = chimp_class_new (NULL, CHIMP_STR_NEW(NULL, "bool"), chimp_object_class);
    if (chimp_bool_class == NULL) goto error;
    chimp_gc_make_root (NULL, chimp_bool_class);
    CHIMP_CLASS(chimp_bool_class)->str = chimp_bool_str;

    chimp_true = chimp_object_new (NULL, chimp_bool_class);
    if (chimp_true == NULL) goto error;
    chimp_gc_make_root (NULL, chimp_true);
    chimp_false = chimp_object_new (NULL, chimp_bool_class);
    if (chimp_false == NULL) goto error;
    chimp_gc_make_root (NULL, chimp_false);

    if (!chimp_int_class_bootstrap (NULL)) goto error;
    if (!chimp_array_class_bootstrap (NULL)) goto error;
    if (!chimp_hash_class_bootstrap (NULL)) goto error;
    if (!chimp_method_class_bootstrap (NULL)) goto error;
    if (!chimp_frame_class_bootstrap ()) goto error;
    if (!chimp_code_class_bootstrap ()) goto error;
    if (!chimp_module_class_bootstrap ()) goto error;

    /*
    if (!chimp_task_push_frame (main_task)) goto error;
    */
    if (!chimp_ast_class_bootstrap ()) goto error;

    return chimp_core_init_builtins ();

error:

    chimp_task_delete (main_task);
    main_task = NULL;
    return CHIMP_FALSE;
}

void
chimp_core_shutdown (void)
{
    if (main_task != NULL) {
        chimp_task_delete (main_task);
        main_task = NULL;
        chimp_object_class = NULL;
        chimp_class_class = NULL;
        chimp_str_class = NULL;
    }
}

void
chimp_bug (const char *filename, int lineno, const char *format, ...)
{
    va_list args;

    fprintf (stderr, "bug: %s:%d: ", filename, lineno);

    va_start (args, format);
    vfprintf (stderr, format, args);
    va_end (args);

    fprintf (stderr, ". aborting ...\n");

    abort ();
}

