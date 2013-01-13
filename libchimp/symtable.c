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

#include "chimp/hash.h"
#include "chimp/array.h"
#include "chimp/class.h"
#include "chimp/object.h"
#include "chimp/symtable.h"

#define CHIMP_SYMTABLE_GET_CURRENT_ENTRY(ref) CHIMP_SYMTABLE(ref)->ste

#define CHIMP_SYMTABLE_ENTRY_CHECK_TYPE(ste, t) \
    ((CHIMP_SYMTABLE_ENTRY(ste)->flags & CHIMP_SYM_TYPE_MASK) == (t))

#define CHIMP_SYMTABLE_ENTRY_IS_MODULE(ste) \
    CHIMP_SYMTABLE_ENTRY_CHECK_TYPE(ste, CHIMP_SYM_MODULE)

#define CHIMP_SYMTABLE_ENTRY_IS_SPAWN(ste) \
    CHIMP_SYMTABLE_ENTRY_CHECK_TYPE(ste, CHIMP_SYM_SPAWN)

ChimpRef *chimp_symtable_class = NULL;
ChimpRef *chimp_symtable_entry_class = NULL;

static chimp_bool_t
chimp_symtable_visit_decls (ChimpRef *self, ChimpRef *decls);

static chimp_bool_t
chimp_symtable_visit_decl (ChimpRef *self, ChimpRef *decl);

static chimp_bool_t
chimp_symtable_visit_stmt (ChimpRef *self, ChimpRef *stmt);

static chimp_bool_t
chimp_symtable_visit_expr (ChimpRef *self, ChimpRef *expr);

static chimp_bool_t
chimp_symtable_visit_mod (ChimpRef *self, ChimpRef *mod);

static ChimpRef *
_chimp_symtable_init (ChimpRef *self, ChimpRef *args)
{
    ChimpRef *filename;
    ChimpRef *ast;

    if (!chimp_method_parse_args (args, "oo", &filename, &ast)) {
        return NULL;
    }

    CHIMP_ANY(self)->klass = chimp_symtable_class;
    CHIMP_SYMTABLE(self)->filename = filename;
    CHIMP_SYMTABLE(self)->ste = chimp_nil;
    CHIMP_SYMTABLE(self)->lookup = chimp_hash_new ();
    if (CHIMP_SYMTABLE(self)->lookup == NULL) {
        return NULL;
    }
    CHIMP_SYMTABLE(self)->stack = chimp_array_new ();
    if (CHIMP_SYMTABLE(self)->stack == NULL) {
        return NULL;
    }

    if (CHIMP_ANY_CLASS(ast) == chimp_ast_mod_class) {
        if (!chimp_symtable_visit_mod (self, ast))
            return NULL;
    }
    else {
        CHIMP_BUG ("unknown top-level AST node type: %s",
                    CHIMP_STR_DATA(CHIMP_CLASS(CHIMP_ANY_CLASS(ast))->name));
        return NULL;
    }

    return self;
}

static void
_chimp_symtable_mark (ChimpGC *gc, ChimpRef *self)
{
    CHIMP_SUPER (self)->mark (gc, self);

    chimp_gc_mark_ref (gc, CHIMP_SYMTABLE(self)->filename);
    chimp_gc_mark_ref (gc, CHIMP_SYMTABLE(self)->lookup);
    chimp_gc_mark_ref (gc, CHIMP_SYMTABLE(self)->stack);
    chimp_gc_mark_ref (gc, CHIMP_SYMTABLE(self)->ste);
}

chimp_bool_t
chimp_symtable_class_bootstrap (void)
{
    chimp_symtable_class = chimp_class_new (
        CHIMP_STR_NEW("symtable"), NULL, sizeof(ChimpSymtable));
    if (chimp_symtable_class == NULL) {
        return CHIMP_FALSE;
    }
    CHIMP_CLASS(chimp_symtable_class)->init = _chimp_symtable_init;
    CHIMP_CLASS(chimp_symtable_class)->mark = _chimp_symtable_mark;
    chimp_gc_make_root (NULL, chimp_symtable_class);
    return CHIMP_TRUE;
}

static ChimpRef *
_chimp_symtable_entry_init (ChimpRef *self, ChimpRef *args)
{
    int64_t flags;
    ChimpRef *symtable;
    ChimpRef *scope;
    ChimpRef *parent;

    if (!chimp_method_parse_args (
            args, "oooI", &symtable, &parent, &scope, &flags)) {
        return NULL;
    }

    CHIMP_SYMTABLE_ENTRY(self)->flags = flags;
    CHIMP_SYMTABLE_ENTRY(self)->symtable = symtable;
    CHIMP_SYMTABLE_ENTRY(self)->scope = scope;
    CHIMP_SYMTABLE_ENTRY(self)->parent = parent;
    CHIMP_SYMTABLE_ENTRY(self)->symbols = chimp_hash_new ();
    if (CHIMP_SYMTABLE_ENTRY(self)->symbols == NULL) {
        CHIMP_BUG ("failed to allocate symbols hash");
        return NULL;
    }
    CHIMP_SYMTABLE_ENTRY(self)->children = chimp_array_new ();
    if (CHIMP_SYMTABLE_ENTRY(self)->children == NULL) {
        CHIMP_BUG ("failed to allocate children hash");
        return NULL;
    }
    return self;
}

static void
_chimp_symtable_entry_mark (ChimpGC *gc, ChimpRef *self)
{
    CHIMP_SUPER (self)->mark (gc, self);

    chimp_gc_mark_ref (gc, CHIMP_SYMTABLE_ENTRY(self)->symtable);
    chimp_gc_mark_ref (gc, CHIMP_SYMTABLE_ENTRY(self)->scope);
    chimp_gc_mark_ref (gc, CHIMP_SYMTABLE_ENTRY(self)->symbols);
    chimp_gc_mark_ref (gc, CHIMP_SYMTABLE_ENTRY(self)->parent);
    chimp_gc_mark_ref (gc, CHIMP_SYMTABLE_ENTRY(self)->children);
}

static void
_chimp_symtable_entry_dtor (ChimpRef *dtor)
{
}

chimp_bool_t
chimp_symtable_entry_class_bootstrap (void)
{
    chimp_symtable_entry_class = chimp_class_new (
        CHIMP_STR_NEW("symtable_entry"), NULL, sizeof(ChimpSymtableEntry));
    if (chimp_symtable_entry_class == NULL) {
        return CHIMP_FALSE;
    }
    CHIMP_CLASS(chimp_symtable_entry_class)->init = _chimp_symtable_entry_init;
    CHIMP_CLASS(chimp_symtable_entry_class)->mark = _chimp_symtable_entry_mark;
    CHIMP_CLASS(chimp_symtable_entry_class)->dtor = _chimp_symtable_entry_dtor;
    chimp_gc_make_root (NULL, chimp_symtable_entry_class);
    return CHIMP_TRUE;
}

static ChimpRef *
chimp_symtable_entry_new (
    ChimpRef *symtable, ChimpRef *parent, ChimpRef *scope, int flags)
{
    ChimpRef *flags_obj = chimp_int_new (flags);
    if (flags_obj == NULL) {
        return NULL;
    }
    symtable = symtable ? symtable : chimp_nil;
    parent = parent ? parent : chimp_nil;
    scope = scope ? scope : chimp_nil;
    return chimp_class_new_instance (
        chimp_symtable_entry_class, symtable, parent, scope, flags_obj, NULL);
}

static chimp_bool_t
chimp_symtable_enter_scope (ChimpRef *self, ChimpRef *scope, int flags)
{
    ChimpRef *ste = CHIMP_SYMTABLE_GET_CURRENT_ENTRY(self);
    ChimpRef *new_ste = chimp_symtable_entry_new (self, ste, scope, flags);
    if (new_ste == NULL) {
        return CHIMP_FALSE;
    }
    if (!chimp_hash_put (CHIMP_SYMTABLE(self)->lookup, scope, new_ste)) {
        return CHIMP_FALSE;
    }
    if (ste != chimp_nil) {
        if (!chimp_array_push (CHIMP_SYMTABLE(self)->stack, ste)) {
            return CHIMP_FALSE;
        }
        if (!chimp_array_push (CHIMP_SYMTABLE_ENTRY_CHILDREN(ste), new_ste)) {
            return CHIMP_FALSE;
        }
    }
    CHIMP_SYMTABLE(self)->ste = new_ste;
    return CHIMP_TRUE;
}

static chimp_bool_t
chimp_symtable_leave_scope (ChimpRef *self)
{
    ChimpRef *stack = CHIMP_SYMTABLE(self)->stack;
    if (CHIMP_ARRAY_SIZE(stack) > 0) {
        CHIMP_SYMTABLE(self)->ste = chimp_array_pop (stack);
    }
    return CHIMP_SYMTABLE_GET_CURRENT_ENTRY(self) != chimp_nil;
}

static chimp_bool_t
chimp_symtable_add (ChimpRef *self, ChimpRef *name, int64_t flags)
{
    ChimpRef *ste = CHIMP_SYMTABLE_GET_CURRENT_ENTRY(self);
    ChimpRef *symbols = CHIMP_SYMTABLE_ENTRY(ste)->symbols;
    ChimpRef *fl = chimp_int_new (flags);
    if (fl == NULL) {
        return CHIMP_FALSE;
    }
    if (!chimp_hash_put (symbols, name, fl)) {
        return CHIMP_FALSE;
    }
    return CHIMP_TRUE;
}

static chimp_bool_t
chimp_symtable_visit_stmts_or_decls (ChimpRef *self, ChimpRef *arr)
{
    size_t i;

    for (i = 0; i < CHIMP_ARRAY_SIZE(arr); i++) {
        ChimpRef *item = CHIMP_ARRAY_ITEM(arr, i);
        if (CHIMP_ANY_CLASS(item) == chimp_ast_stmt_class) {
            if (!chimp_symtable_visit_stmt (self, item)) {
                return CHIMP_FALSE;
            }
        }
        else if (CHIMP_ANY_CLASS(item) == chimp_ast_decl_class) {
            if (!chimp_symtable_visit_decl (self, item)) {
                return CHIMP_FALSE;
            }
        }
        else {
            CHIMP_BUG ("dude");
            return CHIMP_FALSE;
        }
    }
    return CHIMP_TRUE;
}

static chimp_bool_t
chimp_symtable_visit_decl_func (ChimpRef *self, ChimpRef *decl)
{
    ChimpRef *name = CHIMP_AST_DECL(decl)->func.name;
    ChimpRef *args = CHIMP_AST_DECL(decl)->func.args;
    ChimpRef *body = CHIMP_AST_DECL(decl)->func.body;
    ChimpRef *ste = CHIMP_SYMTABLE_GET_CURRENT_ENTRY(self);
    int type = CHIMP_SYMTABLE_ENTRY(ste)->flags & CHIMP_SYM_TYPE_MASK;

    if (!chimp_symtable_add (self, name, CHIMP_SYM_DECL | type)) {
        return CHIMP_FALSE;
    }

    if (!chimp_symtable_enter_scope (self, decl, CHIMP_SYM_FUNC)) {
        return CHIMP_FALSE;
    }

    if (!chimp_symtable_visit_decls (self, args)) {
        return CHIMP_FALSE;
    }

    if (!chimp_symtable_visit_stmts_or_decls (self, body)) {
        return CHIMP_FALSE;
    }

    if (!chimp_symtable_leave_scope (self)) {
        return CHIMP_FALSE;
    }

    return CHIMP_TRUE;
}

static chimp_bool_t
chimp_symtable_visit_decl_class (ChimpRef *self, ChimpRef *decl)
{
    ChimpRef *name = CHIMP_AST_DECL(decl)->class.name;
    ChimpRef *base = CHIMP_AST_DECL(decl)->class.base;
    ChimpRef *body = CHIMP_AST_DECL(decl)->class.body;
    ChimpRef *ste = CHIMP_SYMTABLE_GET_CURRENT_ENTRY(self);
    int type = CHIMP_SYMTABLE_ENTRY(ste)->flags & CHIMP_SYM_TYPE_MASK;

    if (!chimp_symtable_add (self, name, CHIMP_SYM_DECL | type)) {
        return CHIMP_FALSE;
    }

    if (!chimp_symtable_enter_scope (self, decl, CHIMP_SYM_CLASS)) {
        return CHIMP_FALSE;
    }

    if (base != NULL) {
        /* XXX this sucks. */
        if (!chimp_symtable_add (self, chimp_array_first (base), 0)) {
            return CHIMP_FALSE;
        }
    }

    if (!chimp_symtable_visit_decls (self, body)) {
        return CHIMP_FALSE;
    }

    if (!chimp_symtable_leave_scope (self)) {
        return CHIMP_FALSE;
    }

    return CHIMP_TRUE;
}

static chimp_bool_t
chimp_symtable_visit_decl_use (ChimpRef *self, ChimpRef *decl)
{
    ChimpRef *name = CHIMP_AST_DECL(decl)->use.name;
    ChimpRef *ste = CHIMP_SYMTABLE_GET_CURRENT_ENTRY(self);
    int type = CHIMP_SYMTABLE_ENTRY(ste)->flags & CHIMP_SYM_TYPE_MASK;

    if (!chimp_symtable_add (self, name, CHIMP_SYM_DECL | type)) {
        return CHIMP_FALSE;
    }

    return CHIMP_TRUE;
}

static chimp_bool_t
chimp_symtable_visit_decl_var (ChimpRef *self, ChimpRef *decl)
{
    ChimpRef *name = CHIMP_AST_DECL(decl)->var.name;
    ChimpRef *value = CHIMP_AST_DECL(decl)->var.value;
    ChimpRef *ste = CHIMP_SYMTABLE_GET_CURRENT_ENTRY(self);
    int type = CHIMP_SYMTABLE_ENTRY(ste)->flags & CHIMP_SYM_TYPE_MASK;

    if (!chimp_symtable_add (self, name, CHIMP_SYM_DECL | type)) {
        return CHIMP_FALSE;
    }

    if (value != NULL) {
        if (!chimp_symtable_visit_expr (self, value)) {
            return CHIMP_FALSE;
        }
    }

    return CHIMP_TRUE;
}

static chimp_bool_t
chimp_symtable_visit_decl (ChimpRef *self, ChimpRef *decl)
{
    switch (CHIMP_AST_DECL_TYPE(decl)) {
        case CHIMP_AST_DECL_FUNC:
            return chimp_symtable_visit_decl_func (self, decl);
        case CHIMP_AST_DECL_CLASS:
            return chimp_symtable_visit_decl_class (self, decl);
        case CHIMP_AST_DECL_USE:
            return chimp_symtable_visit_decl_use (self, decl);
        case CHIMP_AST_DECL_VAR:
            return chimp_symtable_visit_decl_var (self, decl);
        default:
            CHIMP_BUG ("unknown AST decl type: %d", CHIMP_AST_DECL_TYPE(decl));
            return CHIMP_FALSE;
    }
}

static chimp_bool_t
chimp_symtable_visit_expr_call (ChimpRef *self, ChimpRef *expr)
{
    ChimpRef *target = CHIMP_AST_EXPR(expr)->call.target;
    ChimpRef *args = CHIMP_AST_EXPR(expr)->call.args;
    size_t i;

    if (!chimp_symtable_visit_expr (self, target)) {
        return CHIMP_FALSE;
    }

    for (i = 0; i < CHIMP_ARRAY_SIZE(args); i++) {
        ChimpRef *item = CHIMP_ARRAY_ITEM(args, i);
        if (!chimp_symtable_visit_expr (self, item)) {
            return CHIMP_FALSE;
        }
    }

    return CHIMP_TRUE;
}

static chimp_bool_t
chimp_symtable_visit_expr_getattr (ChimpRef *self, ChimpRef *expr)
{
    ChimpRef *target = CHIMP_AST_EXPR(expr)->getattr.target;

    if (!chimp_symtable_visit_expr (self, target)) {
        return CHIMP_FALSE;
    }

    return CHIMP_TRUE;
}

static chimp_bool_t
chimp_symtable_visit_expr_getitem (ChimpRef *self, ChimpRef *expr)
{
    ChimpRef *target = CHIMP_AST_EXPR(expr)->getitem.target;

    if (!chimp_symtable_visit_expr (self, target)) {
        return CHIMP_FALSE;
    }

    return CHIMP_TRUE;
}

static chimp_bool_t
chimp_symtable_visit_expr_array (ChimpRef *self, ChimpRef *expr)
{
    size_t i;
    ChimpRef *arr = CHIMP_AST_EXPR(expr)->array.value;
    for (i = 0; i < CHIMP_ARRAY_SIZE(arr); i++) {
        ChimpRef *item = CHIMP_ARRAY_ITEM(arr, i);
        if (!chimp_symtable_visit_expr (self, item)) {
            return CHIMP_FALSE;
        }
    }
    return CHIMP_TRUE;
}

static chimp_bool_t
chimp_symtable_visit_expr_hash (ChimpRef *self, ChimpRef *expr)
{
    size_t i;
    ChimpRef *arr = CHIMP_AST_EXPR(expr)->hash.value;
    for (i = 0; i < CHIMP_ARRAY_SIZE(arr); i++) {
        ChimpRef *item = CHIMP_ARRAY_ITEM(arr, i);
        if (!chimp_symtable_visit_expr (self, item)) {
            return CHIMP_FALSE;
        }
    }
    return CHIMP_TRUE;
}

static chimp_bool_t
chimp_symtable_visit_expr_ident (ChimpRef *self, ChimpRef *expr)
{
    ChimpRef *ste = CHIMP_SYMTABLE_GET_CURRENT_ENTRY(self);
    ChimpRef *name = CHIMP_AST_EXPR(expr)->ident.id;

    while (ste != chimp_nil) {
        int rc;
        rc = chimp_hash_get (CHIMP_SYMTABLE_ENTRY(ste)->symbols, name, NULL);
        if (rc == 0) {
            int type = CHIMP_SYMTABLE_ENTRY(ste)->flags &
                        CHIMP_SYM_TYPE_MASK;
            if (ste != CHIMP_SYMTABLE_GET_CURRENT_ENTRY(self)) {
                return chimp_symtable_add (self, name, CHIMP_SYM_FREE | type);
            }
            return CHIMP_TRUE;
        }
        ste = CHIMP_SYMTABLE_ENTRY(ste)->parent;
    }

    /* XXX is this really the best way to handle builtins? */
    if (chimp_is_builtin (name)) {
        return chimp_symtable_add (self, name, CHIMP_SYM_BUILTIN);
    }

    if (strcmp (CHIMP_STR_DATA(name), "__file__") == 0 ||
        strcmp (CHIMP_STR_DATA(name), "__line__") == 0) {
        return chimp_symtable_add (self, name, CHIMP_SYM_SPECIAL);
    }

    CHIMP_BUG ("unknown symbol: %s", CHIMP_STR_DATA(name));
    return CHIMP_FALSE;
}

static chimp_bool_t
chimp_symtable_visit_expr_binop (ChimpRef *self, ChimpRef *expr)
{
    ChimpRef *left = CHIMP_AST_EXPR(expr)->binop.left;
    ChimpRef *right = CHIMP_AST_EXPR(expr)->binop.right;
    if (!chimp_symtable_visit_expr (self, left)) {
        return CHIMP_FALSE;
    }
    if (!chimp_symtable_visit_expr (self, right)) {
        return CHIMP_FALSE;
    }
    return CHIMP_TRUE;
}

static chimp_bool_t
chimp_symtable_visit_expr_fn (ChimpRef *self, ChimpRef *expr)
{
    ChimpRef *args = CHIMP_AST_EXPR(expr)->fn.args;
    ChimpRef *body = CHIMP_AST_EXPR(expr)->fn.body;

    if (!chimp_symtable_enter_scope (self, expr, CHIMP_SYM_FUNC)) {
        return CHIMP_FALSE;
    }

    if (!chimp_symtable_visit_decls (self, args)) {
        return CHIMP_FALSE;
    }

    if (!chimp_symtable_visit_stmts_or_decls (self, body)) {
        return CHIMP_FALSE;
    }

    if (!chimp_symtable_leave_scope (self)) {
        return CHIMP_FALSE;
    }

    return CHIMP_TRUE;
}

static chimp_bool_t
chimp_symtable_visit_expr_spawn (ChimpRef *self, ChimpRef *expr)
{
    ChimpRef *args = CHIMP_AST_EXPR(expr)->spawn.args;
    ChimpRef *body = CHIMP_AST_EXPR(expr)->spawn.body;

    if (!chimp_symtable_enter_scope (self, expr, CHIMP_SYM_SPAWN)) {
        return CHIMP_FALSE;
    }

    if (!chimp_symtable_visit_decls (self, args)) {
        return CHIMP_FALSE;
    }

    if (!chimp_symtable_visit_stmts_or_decls (self, body)) {
        return CHIMP_FALSE;
    }

    if (!chimp_symtable_leave_scope (self)) {
        return CHIMP_FALSE;
    }

    return CHIMP_TRUE;
}

static chimp_bool_t
chimp_symtable_visit_expr_not (ChimpRef *self, ChimpRef *expr)
{
    return chimp_symtable_visit_expr (self, CHIMP_AST_EXPR(expr)->not.value);
}

static chimp_bool_t
chimp_symtable_visit_expr (ChimpRef *self, ChimpRef *expr)
{
    switch (CHIMP_AST_EXPR_TYPE(expr)) {
        case CHIMP_AST_EXPR_CALL:
            return chimp_symtable_visit_expr_call (self, expr);
        case CHIMP_AST_EXPR_GETATTR:
            return chimp_symtable_visit_expr_getattr (self, expr);
        case CHIMP_AST_EXPR_GETITEM:
            return chimp_symtable_visit_expr_getitem (self, expr);
        case CHIMP_AST_EXPR_ARRAY:
            return chimp_symtable_visit_expr_array (self, expr);
        case CHIMP_AST_EXPR_HASH:
            return chimp_symtable_visit_expr_hash (self, expr);
        case CHIMP_AST_EXPR_IDENT:
            return chimp_symtable_visit_expr_ident (self, expr);
        case CHIMP_AST_EXPR_STR:
            return CHIMP_TRUE;
        case CHIMP_AST_EXPR_BOOL:
            return CHIMP_TRUE;
        case CHIMP_AST_EXPR_NIL:
            return CHIMP_TRUE;
        case CHIMP_AST_EXPR_BINOP:
            return chimp_symtable_visit_expr_binop (self, expr);
        case CHIMP_AST_EXPR_INT_:
            return CHIMP_TRUE;
        case CHIMP_AST_EXPR_FLOAT_:
            return CHIMP_TRUE;
        case CHIMP_AST_EXPR_FN:
            return chimp_symtable_visit_expr_fn (self, expr);
        case CHIMP_AST_EXPR_SPAWN:
            return chimp_symtable_visit_expr_spawn (self, expr);
        case CHIMP_AST_EXPR_NOT:
            return chimp_symtable_visit_expr_not (self, expr);
        default:
            CHIMP_BUG ("unknown AST expr type: %d", CHIMP_AST_EXPR_TYPE(expr));
            return CHIMP_FALSE;
    }
}

static chimp_bool_t
chimp_symtable_visit_stmt_assign (ChimpRef *self, ChimpRef *stmt)
{
    ChimpRef *target = CHIMP_AST_STMT(stmt)->assign.target;
    ChimpRef *value = CHIMP_AST_STMT(stmt)->assign.value;

    if (!chimp_symtable_visit_expr (self, target)) {
        return CHIMP_FALSE;
    }

    if (!chimp_symtable_visit_expr (self, value)) {
        return CHIMP_FALSE;
    }

    return CHIMP_TRUE;
}

static chimp_bool_t
chimp_symtable_visit_stmt_if_ (ChimpRef *self, ChimpRef *stmt)
{
    ChimpRef *expr = CHIMP_AST_STMT(stmt)->if_.expr;
    ChimpRef *body = CHIMP_AST_STMT(stmt)->if_.body;
    ChimpRef *orelse = CHIMP_AST_STMT(stmt)->if_.orelse;

    if (!chimp_symtable_visit_expr (self, expr)) {
        return CHIMP_FALSE;
    }

    if (!chimp_symtable_visit_stmts_or_decls (self, body)) {
        return CHIMP_FALSE;
    }

    if (orelse != NULL) {
        if (!chimp_symtable_visit_stmts_or_decls (self, orelse)) {
            return CHIMP_FALSE;
        }
    }

    return CHIMP_TRUE;
}

static chimp_bool_t
chimp_symtable_visit_stmt_while_ (ChimpRef *self, ChimpRef *stmt)
{
    ChimpRef *expr = CHIMP_AST_STMT(stmt)->while_.expr;
    ChimpRef *body = CHIMP_AST_STMT(stmt)->while_.body;

    if (!chimp_symtable_visit_expr (self, expr)) {
        return CHIMP_FALSE;
    }

    if (!chimp_symtable_visit_stmts_or_decls (self, body)) {
        return CHIMP_FALSE;
    }

    return CHIMP_TRUE;
}

static chimp_bool_t
chimp_symtable_visit_stmt_pattern_test (ChimpRef *self, ChimpRef *test)
{
    switch (CHIMP_AST_EXPR(test)->type) {
        case CHIMP_AST_EXPR_IDENT:
            {
                ChimpRef *id = CHIMP_AST_EXPR(test)->ident.id;
                ChimpRef *ste = CHIMP_SYMTABLE_GET_CURRENT_ENTRY(self);
                int type = CHIMP_SYMTABLE_ENTRY(ste)->flags &
                            CHIMP_SYM_TYPE_MASK;
                if (!chimp_symtable_add (self, id, CHIMP_SYM_DECL | type)) {
                    return CHIMP_FALSE;
                }

                break;
            }
        case CHIMP_AST_EXPR_ARRAY:
            {
                ChimpRef *array = CHIMP_AST_EXPR(test)->array.value;
                size_t i;
                for (i = 0; i < CHIMP_ARRAY_SIZE(array); i++) {
                    ChimpRef *item = CHIMP_ARRAY_ITEM(array, i);
                    if (!chimp_symtable_visit_stmt_pattern_test (self, item)) {
                        return CHIMP_FALSE;
                    }
                }

                break;
            }
        case CHIMP_AST_EXPR_HASH:
            {
                ChimpRef *hash = CHIMP_AST_EXPR(test)->hash.value;
                size_t i;
                /* XXX hashes are encoded as arrays in the AST */
                for (i = 0; i < CHIMP_ARRAY_SIZE(hash); i += 2) {
                    ChimpRef *key = CHIMP_ARRAY(hash)->items[i];
                    ChimpRef *value = CHIMP_ARRAY(hash)->items[i+1];
                    if (!chimp_symtable_visit_stmt_pattern_test (self, key)) {
                        return CHIMP_FALSE;
                    }
                    if (!chimp_symtable_visit_stmt_pattern_test (self, value)) {
                        return CHIMP_FALSE;
                    }
                }
            }
        default:
            break;
    };

    return CHIMP_TRUE;
}

static chimp_bool_t
chimp_symtable_visit_stmt_patterns (ChimpRef *self, ChimpRef *patterns)
{
    size_t size = CHIMP_ARRAY_SIZE(patterns);
    size_t i;

    for (i = 0; i < size; i++) {
        ChimpRef *pattern = CHIMP_ARRAY_ITEM(patterns, i);
        ChimpRef *test = CHIMP_AST_STMT(pattern)->pattern.test;
        ChimpRef *body = CHIMP_AST_STMT(pattern)->pattern.body;

        if (!chimp_symtable_visit_stmt_pattern_test (self, test)) {
            return CHIMP_FALSE;
        }

        if (!chimp_symtable_visit_stmts_or_decls (self, body)) {
            return CHIMP_FALSE;
        }
    }

    return CHIMP_TRUE;
}

static chimp_bool_t
chimp_symtable_visit_stmt_match (ChimpRef *self, ChimpRef *stmt)
{
    ChimpRef *expr = CHIMP_AST_STMT(stmt)->match.expr;
    ChimpRef *body = CHIMP_AST_STMT(stmt)->match.body;
    
    if (!chimp_symtable_visit_expr (self, expr)) {
        return CHIMP_FALSE;
    }

    if (!chimp_symtable_visit_stmt_patterns (self, body)) {
        return CHIMP_FALSE;
    }

    return CHIMP_TRUE;
}

static chimp_bool_t
chimp_symtable_visit_stmt_ret (ChimpRef *self, ChimpRef *stmt)
{
    ChimpRef *expr = CHIMP_AST_STMT(stmt)->ret.expr;
    if (expr != NULL) {
        if (!chimp_symtable_visit_expr (self, expr)) {
            return CHIMP_FALSE;
        }
    }
    return CHIMP_TRUE;
}

static chimp_bool_t
chimp_symtable_visit_stmt (ChimpRef *self, ChimpRef *stmt)
{
    switch (CHIMP_AST_STMT_TYPE(stmt)) {
        case CHIMP_AST_STMT_EXPR:
            return chimp_symtable_visit_expr (self, CHIMP_AST_STMT(stmt)->expr.expr);
        case CHIMP_AST_STMT_ASSIGN:
            return chimp_symtable_visit_stmt_assign (self, stmt);
        case CHIMP_AST_STMT_IF_:
            return chimp_symtable_visit_stmt_if_ (self, stmt);
        case CHIMP_AST_STMT_WHILE_:
            return chimp_symtable_visit_stmt_while_ (self, stmt);
        case CHIMP_AST_STMT_MATCH:
            return chimp_symtable_visit_stmt_match (self, stmt);
        case CHIMP_AST_STMT_RET:
            return chimp_symtable_visit_stmt_ret (self, stmt);
        case CHIMP_AST_STMT_BREAK_:
            return CHIMP_TRUE;
        default:
            CHIMP_BUG ("unknown AST stmt type: %d", CHIMP_AST_STMT_TYPE(stmt));
            return CHIMP_FALSE;
    }
}

static chimp_bool_t
chimp_symtable_visit_decls (ChimpRef *self, ChimpRef *decls)
{
    size_t i;

    for (i = 0; i < CHIMP_ARRAY_SIZE(decls); i++) {
        if (!chimp_symtable_visit_decl (self, CHIMP_ARRAY_ITEM(decls, i))) {
            /* TODO error message? */
            return CHIMP_FALSE;
        }
    }
    return CHIMP_TRUE;
}

static chimp_bool_t
chimp_symtable_visit_mod (ChimpRef *self, ChimpRef *mod)
{
    ChimpRef *uses = CHIMP_AST_MOD(mod)->root.uses;
    ChimpRef *body = CHIMP_AST_MOD(mod)->root.body;

    if (!chimp_symtable_enter_scope (self, mod, CHIMP_SYM_MODULE)) {
        return CHIMP_FALSE;
    }

    if (!chimp_symtable_visit_decls (self, uses)) {
        return CHIMP_FALSE;
    }

    if (!chimp_symtable_visit_decls (self, body)) {
        return CHIMP_FALSE;
    }

    if (!chimp_symtable_leave_scope (self)) {
        return CHIMP_FALSE;
    }

    return CHIMP_TRUE;
}

ChimpRef *
chimp_symtable_new_from_ast (ChimpRef *filename, ChimpRef *ast)
{
    return chimp_class_new_instance (chimp_symtable_class, filename, ast, NULL);
}

ChimpRef *
chimp_symtable_lookup (ChimpRef *self, ChimpRef *scope)
{
    ChimpRef *sym;
    int rc = chimp_hash_get (CHIMP_SYMTABLE(self)->lookup, scope, &sym);
    if (rc < 0) {
        CHIMP_BUG ("Error looking up symtable entry for scope %p", scope);
        return NULL;
    }
    else if (rc > 0) {
        CHIMP_BUG ("No symtable entry for scope %p", scope);
        return NULL;
    }
    return sym;
}

chimp_bool_t
chimp_symtable_entry_sym_flags (
    ChimpRef *self, ChimpRef *name, int64_t *flags)
{
    ChimpRef *ste = self;
    chimp_bool_t crossed_spawn_boundary = CHIMP_FALSE;
    while (ste != chimp_nil) {
        ChimpRef *symbols = CHIMP_SYMTABLE_ENTRY(ste)->symbols;
        ChimpRef *ref;
        int rc;
        
        rc = chimp_hash_get (symbols, name, &ref);
        if (rc == 0) {
            if (crossed_spawn_boundary &&
                    !CHIMP_SYMTABLE_ENTRY_IS_MODULE(ste)) {
                /* If we cross a 'spawn' while trying to find this symbol,
                 * the only thing we can legitimately reference is something at
                 * the (immutable) module level.
                 *
                 * Anything else is probably owned by another task & thus not
                 * safe.
                 */
                CHIMP_BUG ("cannot refer to `%s` from another task",
                        CHIMP_STR_DATA(name));
                return CHIMP_FALSE;
            }
            if (flags != NULL) {
                *flags = CHIMP_INT(ref)->value;
            }
            return CHIMP_TRUE;
        }
        if (!crossed_spawn_boundary) {
            crossed_spawn_boundary = CHIMP_SYMTABLE_ENTRY_IS_SPAWN(ste);
        }
        ste = CHIMP_SYMTABLE_ENTRY(ste)->parent;
    }
    return CHIMP_FALSE;
}

chimp_bool_t
chimp_symtable_entry_sym_exists (ChimpRef *self, ChimpRef *name)
{
    return chimp_symtable_entry_sym_flags (self, name, NULL);
}

