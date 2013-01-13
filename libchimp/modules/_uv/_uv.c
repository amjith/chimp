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

typedef struct _ChimpUvTcp {
    ChimpAny base;
    chimp_bool_t live;
    uv_tcp_t tcp;
    ChimpRef *loop;
    ChimpRef *connection_cb;
} ChimpUvTcp;

static ChimpRef *chimp_uv_loop_class = NULL;
static ChimpRef *chimp_uv_tcp_class = NULL;

#define CHIMP_UV_LOOP(ref) \
    CHIMP_CHECK_CAST(ChimpUvLoop, (ref), chimp_uv_loop_class)

#define CHIMP_UV_TCP(ref) \
    CHIMP_CHECK_CAST(ChimpUvTcp, (ref), chimp_uv_tcp_class)

static ChimpRef *
_chimp_uv_loop_init (ChimpRef *self, ChimpRef *args)
{
    if (CHIMP_ARRAY_SIZE(args) > 0) {
        if (CHIMP_ARRAY_ITEM(args, 0) == chimp_true) {
            CHIMP_UV_LOOP(self)->loop = uv_default_loop ();
            /* XXX no idea if this is safe or not */
            CHIMP_UV_LOOP(self)->loop->data = self;
            return self;
        }
    }
    CHIMP_UV_LOOP(self)->loop = uv_loop_new ();
    if (CHIMP_UV_LOOP(self)->loop == NULL) {
        return NULL;
    }
    CHIMP_UV_LOOP(self)->loop->data = self;
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
_chimp_uv_loop_tcp (ChimpRef *self, ChimpRef *args)
{
    return chimp_class_new_instance (chimp_uv_tcp_class, self, NULL);
}

static void
_chimp_uv_loop_walk_mark (uv_handle_t *handle, void *gc)
{
    chimp_gc_mark_ref ((ChimpGC *) gc, (ChimpRef *) handle->data);
}

static void
_chimp_uv_loop_mark (ChimpGC *gc, ChimpRef *self)
{
    uv_walk (CHIMP_UV_LOOP(self)->loop, _chimp_uv_loop_walk_mark, gc);
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
    CHIMP_CLASS(klass)->mark = _chimp_uv_loop_mark;
    if (!chimp_class_add_native_method (klass, "run", _chimp_uv_loop_run)) {
        return NULL;
    }
    if (!chimp_class_add_native_method (klass, "tcp", _chimp_uv_loop_tcp)) {
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
_chimp_uv_tcp_init (ChimpRef *self, ChimpRef *args)
{
    ChimpRef *loop;

    if (!chimp_method_parse_args (args, "o", &loop)) {
        return NULL;
    }

    if (uv_tcp_init (CHIMP_UV_LOOP(loop)->loop, &CHIMP_UV_TCP(self)->tcp) != 0) {
        CHIMP_BUG ("uv_tcp_init failed");
        return NULL;
    }

    CHIMP_UV_TCP(self)->loop = loop;
    CHIMP_UV_TCP(self)->tcp.data = self;
    CHIMP_UV_TCP(self)->live = CHIMP_TRUE;

    return self;
}

static void
_chimp_uv_tcp_dtor (ChimpRef *self)
{
    if (CHIMP_UV_TCP(self)->live) {
        uv_close ((uv_handle_t *)(&CHIMP_UV_TCP(self)->tcp), NULL);
        CHIMP_UV_TCP(self)->live = CHIMP_FALSE;
    }
}

static ChimpRef *
_chimp_uv_tcp_bind (ChimpRef *self, ChimpRef *args)
{
    struct sockaddr_in addr;
    const char *host;
    int64_t port;

    if (CHIMP_ARRAY_SIZE(args) == 1) {
        if (!chimp_method_parse_args (args, "I", &port)) {
            return NULL;
        }
        host = "0.0.0.0";
    }
    else {
        if (!chimp_method_parse_args (args, "sI", &host, &port)) {
            return NULL;
        }
    }

    addr = uv_ip4_addr (host, (int) port);
    if (addr.sin_addr.s_addr == INADDR_NONE) {
        CHIMP_BUG ("invalid host or port");
        return NULL;
    }

    if (uv_tcp_bind (&CHIMP_UV_TCP(self)->tcp, addr) != 0) {
        CHIMP_BUG ("uv_tcp_bind failed");
        return NULL;
    }

    return self;
}

static void
_chimp_uv_tcp_connection_cb (uv_stream_t *srv, int status)
{
    ChimpRef *args;
    ChimpRef *self = srv->data;
    ChimpRef *client =
        chimp_class_new_instance (chimp_uv_tcp_class, srv->loop->data, NULL);
    if (client == NULL) {
        return;
    }
    if (uv_accept (srv, (uv_stream_t *)&CHIMP_UV_TCP(client)->tcp) != 0) {
        return;
    }
    args = chimp_array_new_var (client, NULL);
    if (args == NULL) {
        CHIMP_UV_TCP(client)->live = CHIMP_FALSE;
        uv_close ((uv_handle_t *)&CHIMP_UV_TCP(client)->tcp, NULL);
        return;
    }
    if (chimp_object_call (CHIMP_UV_TCP(self)->connection_cb, args) == NULL) {
        CHIMP_UV_TCP(client)->live = CHIMP_FALSE;
        uv_close ((uv_handle_t *)&CHIMP_UV_TCP(client)->tcp, NULL);
        return;
    }
}

static ChimpRef *
_chimp_uv_tcp_listen (ChimpRef *self, ChimpRef *args)
{
    int64_t backlog;
    ChimpRef *cb;

    if (CHIMP_ARRAY_SIZE(args) == 1) {
        if (!chimp_method_parse_args (args, "o", &cb)) {
            return NULL;
        }
        backlog = SOMAXCONN;
    }
    else {
        if (!chimp_method_parse_args (args, "Io", &backlog, &cb)) {
            return NULL;
        }
    }

    /* XXX this kind of sucks */
    CHIMP_UV_TCP(self)->connection_cb = cb;

    if (uv_listen (
            (uv_stream_t *)&CHIMP_UV_TCP(self)->tcp, (int) backlog,
            _chimp_uv_tcp_connection_cb) != 0) {
        CHIMP_BUG ("uv_listen failed");
        return NULL;
    }
    return self;
}

static ChimpRef *
_chimp_uv_tcp_close (ChimpRef *self, ChimpRef *args)
{
    if (CHIMP_UV_TCP(self)->live) {
        CHIMP_UV_TCP(self)->live = CHIMP_FALSE;
        uv_close ((uv_handle_t *)&CHIMP_UV_TCP(self)->tcp, NULL);
    }
    return chimp_nil;
}

static void
_chimp_uv_tcp_mark (ChimpGC *gc, ChimpRef *self)
{
    chimp_gc_mark_ref (gc, CHIMP_UV_TCP(self)->connection_cb);
    chimp_gc_mark_ref (gc, CHIMP_UV_TCP(self)->loop);
}

static ChimpRef *
_chimp_init_uv_tcp_class (void)
{
    ChimpRef *klass =
        chimp_class_new (CHIMP_STR_NEW("_uv.tcp"), NULL, sizeof (ChimpUvTcp));
    if (klass == NULL) {
        return NULL;
    }
    CHIMP_CLASS(klass)->init = _chimp_uv_tcp_init;
    CHIMP_CLASS(klass)->dtor = _chimp_uv_tcp_dtor;
    CHIMP_CLASS(klass)->mark = _chimp_uv_tcp_mark;
    if (!chimp_class_add_native_method (klass, "bind", _chimp_uv_tcp_bind)) {
        return NULL;
    }
    if (!chimp_class_add_native_method (klass, "listen", _chimp_uv_tcp_listen)) {
        return NULL;
    }
    if (!chimp_class_add_native_method (klass, "close", _chimp_uv_tcp_close)) {
        return NULL;
    }
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
    ChimpRef *temp;

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

    temp = _chimp_init_uv_tcp_class ();
    if (temp == NULL)
        return NULL;
    chimp_uv_tcp_class = temp;
    if (!chimp_module_add_local_str (_uv, "tcp", chimp_uv_tcp_class)) {
        return NULL;
    }

    temp = _chimp_init_uv_loop_class ();
    if (temp == NULL)
        return NULL;
    chimp_uv_loop_class = temp;
    if (!chimp_module_add_local_str (_uv, "loop", chimp_uv_loop_class))
        return NULL;

    if (!chimp_module_add_method_str (
            _uv, "default_loop", _chimp_uv_default_loop)) {
        return NULL;
    }

    return _uv;
}


