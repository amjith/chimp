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

#include <stdio.h>

#include "uv.h"
#include "chimp/any.h"
#include "chimp/object.h"
#include "chimp/array.h"
#include "chimp/str.h"

typedef struct _ChimpUvLoop {
    ChimpAny base;
    uv_loop_t *loop;
} ChimpUvLoop;

static ChimpRef *chimp_uv_loop_class = NULL;

#define CHIMP_UV_LOOP(ref) \
    CHIMP_CHECK_CAST(ChimpUvLoop, (ref), chimp_uv_loop_class)

static ChimpRef *
_chimp_uv_loop_init (ChimpRef *self, ChimpRef *args)
{
    if (CHIMP_ARRAY_SIZE(args) > 0) {
        if (CHIMP_ARRAY_ITEM(args, 0) == chimp_true) {
            CHIMP_UV_LOOP(self)->loop = uv_default_loop ();
            return self;
        }
    }
    CHIMP_UV_LOOP(self)->loop = uv_loop_new ();
    if (CHIMP_UV_LOOP(self)->loop == NULL) {
        return NULL;
    }
    return self;
}

static void
_chimp_uv_loop_dtor (ChimpRef *self)
{
    if (CHIMP_UV_LOOP(self)->loop != NULL) {
        if (CHIMP_UV_LOOP(self)->loop != uv_default_loop ()) {
            uv_loop_delete (CHIMP_UV_LOOP(self)->loop);
            CHIMP_UV_LOOP(self)->loop = NULL;
        }
    }
}

static ChimpRef *
_chimp_uv_loop_run (ChimpRef *self, ChimpRef *args)
{
    if (uv_run (CHIMP_UV_LOOP(self)->loop) != 0) {
        CHIMP_BUG ("failed to run uv loop");
        return NULL;
    }
    return chimp_nil;
}

static ChimpRef *
_chimp_init_uv_loop_class (void)
{
    ChimpRef *klass =
        chimp_class_new (CHIMP_STR_NEW("_uv.loop"), NULL, sizeof(ChimpUvLoop));
    if (klass == NULL) {
        return NULL;
    }
    CHIMP_CLASS(klass)->init = _chimp_uv_loop_init;
    CHIMP_CLASS(klass)->dtor = _chimp_uv_loop_dtor;
    if (!chimp_class_add_native_method (klass, "run", _chimp_uv_loop_run)) {
        return NULL;
    }
#if 0
    if (!chimp_class_add_native_method (klass, "read", _chimp_io_file_read))
        return NULL;
    if (!chimp_class_add_native_method (klass, "write", _chimp_io_file_write))
        return NULL;
    if (!chimp_class_add_native_method (klass, "close", _chimp_io_file_close))
        return NULL;
#endif
    return klass;
}

static ChimpRef *
_chimp_uv_default_loop (ChimpRef *self, ChimpRef *args)
{
    return chimp_class_new_instance (chimp_uv_loop_class, chimp_true, NULL);
}

ChimpRef *
chimp_init_uv_module (void)
{
    ChimpRef *_uv;
    ChimpRef *uv_loop_class;

    _uv = chimp_module_new_str ("_uv", NULL);
    if (_uv == NULL) {
        return NULL;
    }

#if 0
    if (!chimp_module_add_method_str (_uv, "print", _chimp_io_print)) {
        return NULL;
    }

    if (!chimp_module_add_method_str (_uv, "write", _chimp_io_write)) {
        return NULL;
    }

    if (!chimp_module_add_method_str (_uv, "readline", _chimp_io_readline)) {
        return NULL;
    }
#endif

    uv_loop_class = _chimp_init_uv_loop_class ();
    if (uv_loop_class == NULL)
        return NULL;
    chimp_uv_loop_class = uv_loop_class;
    
    if (!chimp_module_add_local_str (_uv, "loop", chimp_uv_loop_class))
        return NULL;

    if (!chimp_module_add_method_str (
            _uv, "default_loop", _chimp_uv_default_loop)) {
        return NULL;
    }

    return _uv;
}


