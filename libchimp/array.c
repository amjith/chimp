#include "chimp/array.h"
#include "chimp/object.h"
#include "chimp/core.h"
#include "chimp/class.h"
#include "chimp/str.h"

#define CHIMP_ARRAY_INIT(ref) \
    CHIMP_ANY(ref)->type = CHIMP_VALUE_TYPE_ARRAY; \
    CHIMP_ANY(ref)->klass = chimp_array_class;

ChimpRef *chimp_array_class = NULL;

static ChimpRef *
_chimp_array_push (ChimpRef *self, ChimpRef *args)
{
    /* TODO error if args len == 0 */
    if (!chimp_array_push (self, chimp_array_get (args, 0))) {
        /* XXX error? exception? abort? */
        return NULL;
    }
    return chimp_nil;
}

static ChimpRef *
_chimp_array_pop (ChimpRef *self, ChimpRef *args)
{
    return chimp_array_pop (self);
}

static ChimpRef *
_chimp_array_map (ChimpRef *self, ChimpRef *args)
{
    size_t i;
    ChimpRef *result = chimp_array_new ();
    ChimpRef *fn = CHIMP_ARRAY_ITEM(args, 0);
    for (i = 0; i < CHIMP_ARRAY_SIZE(self); i++) {
        ChimpRef *fn_args;
        ChimpRef *mapped;
        ChimpRef *value = CHIMP_ARRAY_ITEM(self, i);

        fn_args = chimp_array_new_var (value, NULL);
        if (fn_args == NULL) {
            return NULL;
        }
        mapped = chimp_object_call (fn, fn_args);
        if (mapped == NULL) {
            return NULL;
        }
        if (!chimp_array_push (result, mapped)) {
            return NULL;
        }
    }
    return result;
}

static ChimpRef *
_chimp_array_each (ChimpRef *self, ChimpRef *args)
{
    size_t i;
    ChimpRef *fn = CHIMP_ARRAY_ITEM(args, 0);
    for (i = 0; i < CHIMP_ARRAY_SIZE(self); i++) {
        ChimpRef *fn_args;
        ChimpRef *value;
        
        value = CHIMP_ARRAY_ITEM(self, i);
        fn_args = chimp_array_new_var (value, NULL);
        if (fn_args == NULL) {
            return NULL;
        }
        if (chimp_object_call (fn, fn_args) == NULL) {
            return NULL;
        }
    }
    return chimp_nil;
}

static ChimpRef *
_chimp_array_contains (ChimpRef *self, ChimpRef *args)
{
    size_t i;
    ChimpRef *right = CHIMP_ARRAY_ITEM(args, 0);
    for (i = 0; i < CHIMP_ARRAY_SIZE(self); i++) {
        ChimpCmpResult r;
        ChimpRef *left = CHIMP_ARRAY_ITEM(self, i);
        
        r = chimp_object_cmp (left, right);
        if (r == CHIMP_CMP_ERROR) {
            return NULL;
        }
        else if (r == CHIMP_CMP_EQ) {
            return chimp_true;
        }
    }
    return chimp_false;
}

static ChimpRef *
_chimp_array_filter (ChimpRef *self, ChimpRef *args)
{
    size_t i;
    ChimpRef *result;
    ChimpRef *fn = CHIMP_ARRAY_ITEM(args, 0);

    result = chimp_array_new ();
    for (i = 0; i < CHIMP_ARRAY_SIZE(self); i++) {
        ChimpRef *value = CHIMP_ARRAY_ITEM(self, i);
        ChimpRef *fn_args;
        ChimpRef *r;
        
        fn_args = chimp_array_new_var (value, NULL);
        if (fn_args == NULL) {
            return NULL;
        }
        r = chimp_object_call (fn, fn_args);
        if (r == NULL) {
            return NULL;
        }
        if (r == chimp_true) {
            if (!chimp_array_push (result, value)) {
                return NULL;
            }
        }
    }
    return result;
}

static ChimpRef *
chimp_array_str (ChimpRef *self)
{
    size_t size = CHIMP_ARRAY_SIZE(self);
    /* '[' + ']' + (', ' x (size-1)) + '\0' */
    size_t total_len = 2 + (size > 0 ? ((size-1) * 2) : 0) + 1;
    ChimpRef *ref;
    ChimpRef *item_strs;
    char *data;
    size_t i, j;


    item_strs = chimp_array_new ();
    if (item_strs == NULL) {
        return NULL;
    }

    for (i = 0; i < size; i++) {
        ref = CHIMP_ARRAY_ITEM(self, i);
        /* XXX what we really want is something like Python's repr() */
        if (CHIMP_ANY_TYPE(ref) == CHIMP_VALUE_TYPE_STR) {
            /* for surrounding quotes */
            total_len += 2;
        }
        ref = chimp_object_str (ref);
        if (ref == NULL) {
            return NULL;
        }
        chimp_array_push (item_strs, ref);
        total_len += CHIMP_STR_SIZE(ref);
    }

    data = CHIMP_MALLOC(char, total_len);
    if (data == NULL) {
        return NULL;
    }
    j = 0;
    data[j++] = '[';

    for (i = 0; i < size; i++) {
        ref = CHIMP_ARRAY_ITEM(item_strs, i);
        /* XXX what we really want is something like Python's repr() */
        if (CHIMP_ANY_TYPE(CHIMP_ARRAY_ITEM(self, i)) == CHIMP_VALUE_TYPE_STR) {
            data[j++] = '"';
        }
        memcpy (data + j, CHIMP_STR_DATA(ref), CHIMP_STR_SIZE(ref));
        j += CHIMP_STR_SIZE(ref);
        if (CHIMP_ANY_TYPE(CHIMP_ARRAY_ITEM(self, i)) == CHIMP_VALUE_TYPE_STR) {
            data[j++] = '"';
        }
        if (i < (size-1)) {
            memcpy (data + j, ", ", 2);
            j += 2;
        }
    }

    data[j++] = ']';
    data[j] = '\0';

    return chimp_str_new_take (data, total_len-1);
}

chimp_bool_t
chimp_array_class_bootstrap (void)
{
    chimp_array_class =
        chimp_class_new (CHIMP_STR_NEW("array"), chimp_object_class);
    if (chimp_array_class == NULL) {
        return CHIMP_FALSE;
    }
    CHIMP_CLASS(chimp_array_class)->str = chimp_array_str;
    CHIMP_CLASS(chimp_array_class)->inst_type = CHIMP_VALUE_TYPE_ARRAY;
    chimp_gc_make_root (NULL, chimp_array_class);
    chimp_class_add_native_method (chimp_array_class, "push", _chimp_array_push);
    chimp_class_add_native_method (chimp_array_class, "pop", _chimp_array_pop);
    chimp_class_add_native_method (chimp_array_class, "map", _chimp_array_map);
    chimp_class_add_native_method (chimp_array_class, "filter", _chimp_array_filter);
    chimp_class_add_native_method (chimp_array_class, "each", _chimp_array_each);
    chimp_class_add_native_method (chimp_array_class, "contains", _chimp_array_contains);
    return CHIMP_TRUE;
}

ChimpRef *
chimp_array_new (void)
{
    ChimpRef *ref = chimp_gc_new_object (NULL);
    if (ref == NULL) {
        return NULL;
    }
    CHIMP_ARRAY_INIT(ref);
    CHIMP_ARRAY(ref)->items = NULL;
    CHIMP_ARRAY(ref)->size = 0;
    return ref;
}

ChimpRef *
chimp_array_new_var (ChimpRef *a, ...)
{
    va_list args;
    ChimpRef *arg;
    ChimpRef *ref = chimp_array_new ();
    if (ref == NULL) {
        return NULL;
    }

    va_start (args, a);
    if (!chimp_array_push (ref, a)) {
        return NULL;
    }
    while ((arg = va_arg (args, ChimpRef *)) != NULL) {
        chimp_array_push (ref, arg);
    }
    va_end (args);

    return ref;
}

static chimp_bool_t
chimp_array_grow (ChimpRef *self)
{
    ChimpRef **items;
    ChimpArray *arr = CHIMP_ARRAY(self);
    items = CHIMP_REALLOC(ChimpRef *, arr->items, sizeof(*arr->items) * (arr->size + 1));
    if (items == NULL) {
        return CHIMP_FALSE;
    }
    arr->items = items;
    return CHIMP_TRUE;
}

chimp_bool_t
chimp_array_unshift (ChimpRef *self, ChimpRef *value)
{
    size_t i;
    ChimpArray *arr = CHIMP_ARRAY(self);
    if (!chimp_array_grow (self)) {
        return CHIMP_FALSE;
    }
    if (arr->size > 0) {
        for (i = arr->size; i > 0; i--) {
            arr->items[i] = arr->items[i-1];
        }
    }
    arr->items[0] = value;
    arr->size++;
    return CHIMP_TRUE;
}

chimp_bool_t
chimp_array_push (ChimpRef *self, ChimpRef *value)
{
    ChimpArray *arr = CHIMP_ARRAY(self);
    if (!chimp_array_grow (self)) {
        return CHIMP_FALSE;
    }
    arr->items[arr->size++] = value;
    return CHIMP_TRUE;
}

ChimpRef *
chimp_array_pop (ChimpRef *self)
{
    ChimpArray *arr = CHIMP_ARRAY(self);
    if (arr->size > 0) {
        return arr->items[--arr->size];
    }
    else {
        return chimp_nil;
    }
}

ChimpRef *
chimp_array_get (ChimpRef *self, int32_t pos)
{
    ChimpArray *arr = CHIMP_ARRAY(self);
    if (pos >= 0) {
        if (pos < arr->size) {
            return arr->items[pos];
        }
        else {
            return chimp_nil;
        }
    }
    else {
        if ((size_t)(-pos) <= arr->size) {
            return arr->items[arr->size - (size_t)(-pos)];
        }
        else {
            return chimp_nil;
        }
    }
}

int32_t
chimp_array_find (ChimpRef *self, ChimpRef *value)
{
    size_t i;
    for (i = 0; i < CHIMP_ARRAY_SIZE(self); i++) {
        ChimpCmpResult r = chimp_object_cmp (CHIMP_ARRAY_ITEM(self, i), value);
        if (r == CHIMP_CMP_ERROR) {
            return -1;
        }
        else if (r == CHIMP_CMP_EQ) {
            return (int32_t)i;
        }
    }
    return -1;
}

