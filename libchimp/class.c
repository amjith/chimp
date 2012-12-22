#include "chimp/gc.h"
#include "chimp/object.h"
#include "chimp/class.h"
#include "chimp/lwhash.h"
#include "chimp/str.h"
#include "chimp/method.h"

#define CHIMP_CLASS_INIT(ref) \
    CHIMP_ANY(ref)->type = CHIMP_VALUE_TYPE_CLASS; \
    CHIMP_ANY(ref)->klass = chimp_class_class;

static ChimpRef *
chimp_class_call (ChimpRef *self, ChimpRef *args)
{
    ChimpRef *ctor;
    ChimpRef *ref;
    
    /* XXX having two code paths here is probably wrong/begging for trouble */
    if (self == chimp_class_class) {
        ref = CHIMP_ANY_CLASS(CHIMP_ARRAY_ITEM(args, 0));
    }
    else {
        ref = chimp_gc_new_object (NULL);
        if (ref == NULL) {
            return NULL;
        }
        CHIMP_ANY(ref)->klass = self;
        CHIMP_ANY(ref)->type = CHIMP_CLASS(self)->inst_type;
        if (CHIMP_CLASS(self)->init != NULL) {
            ref = CHIMP_CLASS(self)->init (ref, args);
            if (ref == NULL) {
                return NULL;
            }
        }
        else {
            ctor = chimp_object_getattr_str (ref, "init");
            if (ctor != NULL && ctor != chimp_nil) {
                ref = chimp_object_call (ctor, args);
                if (ref == NULL) {
                    return NULL;
                }
            }
        }
    }
    return ref;
}

static const char empty[] = "";

static ChimpRef *
chimp_str_init (ChimpRef *self, ChimpRef *args)
{
    /* TODO don't copy string data on every call ... */
    if (CHIMP_ARRAY_SIZE(args) == 0) {
        CHIMP_STR(self)->data = strdup (empty);
        CHIMP_STR(self)->size = 0;
    }
    else if (CHIMP_ARRAY_SIZE(args) == 1) {
        ChimpRef *temp = CHIMP_ARRAY_FIRST(args);
        /* optimization for str("some string") */
        if (CHIMP_ANY_CLASS(temp) == chimp_str_class) {
            return temp;
        }
        temp = chimp_object_str (CHIMP_ARRAY_FIRST(args));
        CHIMP_STR(self)->data = strdup (CHIMP_STR_DATA(temp));
        CHIMP_STR(self)->size = strlen (CHIMP_STR_DATA(self));
    }
    else {
        size_t i;
        size_t len;
        char *p;
        ChimpRef *str_values = chimp_array_new ();
        if (str_values == NULL) {
            return NULL;
        }

        /* 1. convert all constructor args to strings */
        for (i = 0; i < CHIMP_ARRAY_SIZE(args); i++) {
            ChimpRef *str = CHIMP_ARRAY_ITEM(args, i);
            if (CHIMP_ANY_CLASS(str) != chimp_str_class) {
                str = chimp_object_str (CHIMP_ARRAY_ITEM(args, i));
            }
            if (str == NULL) {
                return NULL;
            }
            if (!chimp_array_push (str_values, str)) {
                return NULL;
            }
        }

        /* 2. compute total length of the arg strings */
        len = 0;
        for (i = 0; i < CHIMP_ARRAY_SIZE(str_values); i++) {
            ChimpRef *str = CHIMP_ARRAY_ITEM(str_values, i);
            if (str == NULL) {
                return NULL;
            }
            len += CHIMP_STR_SIZE(str);
        }

        /* 3. allocate buffer & copy arg strings into it */
        CHIMP_STR(self)->data = CHIMP_MALLOC (char, len + 1);
        if (CHIMP_STR(self)->data == NULL) {
            return NULL;
        }
        CHIMP_STR(self)->size = len;
        p = CHIMP_STR(self)->data;
        for (i = 0; i < CHIMP_ARRAY_SIZE(str_values); i++) {
            ChimpRef *str = CHIMP_ARRAY_ITEM(str_values, i);
            if (str == NULL) {
                return NULL;
            }
            memcpy (p, CHIMP_STR_DATA(str), CHIMP_STR_SIZE(str));
            p += CHIMP_STR_SIZE(str);
        }
        *p = '\0';
    }
    return self;
}

static void
chimp_str_dtor (ChimpRef *self)
{
    CHIMP_FREE(CHIMP_STR(self)->data);
}

ChimpRef *
chimp_class_new (ChimpRef *name, ChimpRef *super)
{
    ChimpRef *ref = chimp_gc_new_object (NULL);
    if (ref == NULL) {
        return NULL;
    }
    if (super == NULL || super == chimp_nil) {
        super = chimp_object_class;
    }
    CHIMP_CLASS_INIT(ref);
    CHIMP_CLASS(ref)->name = name;
    CHIMP_CLASS(ref)->super = super;
    CHIMP_CLASS(ref)->methods = chimp_lwhash_new ();
    CHIMP_CLASS(ref)->call = chimp_class_call;
    CHIMP_CLASS(ref)->inst_type = CHIMP_VALUE_TYPE_OBJECT;
    return ref;
}

ChimpRef *
chimp_class_new_instance (ChimpRef *klass, ...)
{
    va_list args;
    ChimpRef *arg;
    ChimpRef *arr = chimp_array_new ();
    if (arr == NULL) {
        return NULL;
    }

    va_start (args, klass);
    while ((arg = va_arg(args, ChimpRef *)) != NULL) {
        if (!chimp_array_push (arr, arg)) {
            va_end (args);
            return NULL;
        }
    }
    va_end (args);

    return chimp_class_call (klass, arr);
}

chimp_bool_t
chimp_class_add_method (ChimpRef *self, ChimpRef *name, ChimpRef *method)
{
    return chimp_lwhash_put (CHIMP_CLASS(self)->methods, name, method);
}

chimp_bool_t
chimp_class_add_native_method (ChimpRef *self, const char *name, ChimpNativeMethodFunc func)
{
    ChimpRef *method_ref;
    ChimpRef *name_ref = chimp_str_new (name, strlen (name));
    if (name_ref == NULL) {
        return CHIMP_FALSE;
    }
    /* XXX the module should strictly be the module in which this class is declared */
    method_ref = chimp_method_new_native (NULL, func);
    if (method_ref == NULL) {
        return CHIMP_FALSE;
    }
    return chimp_class_add_method (self, name_ref, method_ref);
}

static void
chimp_class_dtor (ChimpRef *self)
{
    chimp_lwhash_delete (CHIMP_CLASS(self)->methods);
}

static ChimpRef *
chimp_class_getattr (ChimpRef *self, ChimpRef *attr)
{
    if (strcmp (CHIMP_STR_DATA(attr), "name") == 0) {
        return CHIMP_CLASS(self)->name;
    }
    else if (strcmp (CHIMP_STR_DATA(attr), "super") == 0) {
        if (CHIMP_CLASS(self)->super == NULL) {
            return chimp_nil;
        }
        else {
            return CHIMP_CLASS(self)->super;
        }
    }
    else {
        return NULL;
    }
}

static ChimpRef *
chimp_class_str (ChimpRef *self)
{
    return chimp_str_new_concat (
        "<class '", CHIMP_STR_DATA(CHIMP_CLASS(self)->name), "'>", NULL);
}

chimp_bool_t
_chimp_bootstrap_L3 (void)
{
    CHIMP_CLASS(chimp_class_class)->methods = chimp_lwhash_new ();
    CHIMP_CLASS(chimp_class_class)->call = chimp_class_call;
    CHIMP_CLASS(chimp_class_class)->inst_type = CHIMP_VALUE_TYPE_CLASS;
    CHIMP_CLASS(chimp_class_class)->dtor = chimp_class_dtor;
    CHIMP_CLASS(chimp_class_class)->getattr = chimp_class_getattr;
    CHIMP_CLASS(chimp_class_class)->str = chimp_class_str;

    CHIMP_CLASS(chimp_object_class)->methods = chimp_lwhash_new ();
    CHIMP_CLASS(chimp_object_class)->call = chimp_class_call;
    CHIMP_CLASS(chimp_object_class)->inst_type = CHIMP_VALUE_TYPE_OBJECT;

    CHIMP_CLASS(chimp_str_class)->methods = chimp_lwhash_new ();
    CHIMP_CLASS(chimp_str_class)->call = chimp_class_call;
    CHIMP_CLASS(chimp_str_class)->inst_type = CHIMP_VALUE_TYPE_STR;
    CHIMP_CLASS(chimp_str_class)->init = chimp_str_init;
    CHIMP_CLASS(chimp_str_class)->dtor = chimp_str_dtor;

    return CHIMP_TRUE;
}

