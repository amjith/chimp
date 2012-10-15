#include "chimp/code.h"
#include "chimp/str.h"
#include "chimp/class.h"
#include "chimp/array.h"
#include "chimp/int.h"

ChimpRef *chimp_code_class = NULL;

chimp_bool_t
chimp_code_class_bootstrap (void)
{
    chimp_code_class =
        chimp_class_new (NULL, CHIMP_STR_NEW(NULL, "code"), NULL);
    if (chimp_code_class == NULL) {
        return CHIMP_FALSE;
    }
    chimp_gc_make_root (NULL, chimp_code_class);
    return CHIMP_TRUE;
}

ChimpRef *
chimp_code_new (void)
{
    ChimpRef *ref = chimp_gc_new_object (NULL);
    ChimpRef *temp;
    if (ref == NULL) {
        return NULL;
    }
    CHIMP_ANY(ref)->type = CHIMP_VALUE_TYPE_CODE;
    CHIMP_ANY(ref)->klass = chimp_code_class;
    temp = chimp_array_new (NULL);
    if (temp == NULL) {
        chimp_bug (__FILE__, __LINE__, "wtf");
        return NULL;
    }
    CHIMP_CODE(ref)->constants = temp;
    temp = chimp_array_new (NULL);
    if (temp == NULL) {
        chimp_bug (__FILE__, __LINE__, "wtf");
        return NULL;
    }
    CHIMP_CODE(ref)->names = temp;
    CHIMP_CODE(ref)->allocated = 256;
    CHIMP_CODE(ref)->bytecode = CHIMP_MALLOC(uint32_t, sizeof(uint32_t) * CHIMP_CODE(ref)->allocated);
    if (CHIMP_CODE(ref)->bytecode == NULL) {
        return NULL;
    }
    return ref;
}

#define CHIMP_CURR_INSTR(co) CHIMP_CODE(co)->bytecode[CHIMP_CODE(co)->used]
#define CHIMP_NEXT_INSTR(co) CHIMP_CODE(co)->bytecode[CHIMP_CODE(co)->used++]

#define CHIMP_MAKE_INSTR0(op) \
    (((CHIMP_OPCODE_ ## op) & 0xff) << 24)

#define CHIMP_MAKE_INSTR1(op, arg1) \
    ((((CHIMP_OPCODE_ ## op) & 0xff) << 24) | (((arg1) & 0xff) << 16))

#define CHIMP_MAKE_INSTR2(op, arg1, arg2) \
    ((((CHIMP_OPCODE_ ## op) & 0xff) << 24) | (((arg1) & 0xff) << 16) | (((arg2) & 0xff) << 8))

#define CHIMP_MAKE_INSTR3(op, arg1, arg2, arg3) \
    ((((CHIMP_OPCODE_ ## op) & 0xff) << 24) | (((arg1) & 0xff) << 16) | (((arg2) & 0xff) << 8) | ((arg3) & 0xff))

static chimp_bool_t
chimp_code_grow (ChimpRef *self)
{
    uint32_t *bytecode;
    if (CHIMP_CODE(self)->used < CHIMP_CODE(self)->allocated) {
        return CHIMP_TRUE;
    }
    bytecode = CHIMP_REALLOC(uint32_t, CHIMP_CODE(self)->bytecode, sizeof(uint32_t) * (CHIMP_CODE(self)->allocated + 32));
    if (bytecode == NULL) {
        return CHIMP_FALSE;
    }
    CHIMP_CODE(self)->bytecode = bytecode;
    CHIMP_CODE(self)->allocated += 32;
    return CHIMP_TRUE;
}

static int32_t
chimp_code_add_const (ChimpRef *self, ChimpRef *value)
{
    int32_t n = chimp_array_find (CHIMP_CODE(self)->constants, value);
    if (n >= 0) {
        return n;
    }
    if (!chimp_array_push (CHIMP_CODE(self)->constants, value)) {
        return -1;
    }
    return CHIMP_ARRAY_SIZE(CHIMP_CODE(self)->constants) - 1;
}

static int32_t
chimp_code_add_name (ChimpRef *self, ChimpRef *id)
{
    int32_t n = chimp_array_find (CHIMP_CODE(self)->names, id);
    if (n >= 0) {
        return n;
    }
    if (!chimp_array_push (CHIMP_CODE(self)->names, id)) {
        return -1;
    }
    return CHIMP_ARRAY_SIZE(CHIMP_CODE(self)->names) - 1;
}

chimp_bool_t
chimp_code_pushconst (ChimpRef *self, ChimpRef *value)
{
    int32_t arg;
    if (!chimp_code_grow (self)) {
        return CHIMP_FALSE;
    }
    arg = chimp_code_add_const (self, value);
    if (arg < 0) {
        return CHIMP_FALSE;
    }
    CHIMP_NEXT_INSTR(self) = CHIMP_MAKE_INSTR1(PUSHCONST, arg);
    return CHIMP_TRUE;
}

chimp_bool_t
chimp_code_pushname (ChimpRef *self, ChimpRef *id)
{
    int32_t arg;
    if (!chimp_code_grow (self)) {
        return CHIMP_FALSE;
    }
    arg = chimp_code_add_name (self, id);
    if (arg < 0) {
        return CHIMP_FALSE;
    }
    CHIMP_NEXT_INSTR(self) = CHIMP_MAKE_INSTR1(PUSHNAME, arg);
    return CHIMP_TRUE;
}

chimp_bool_t
chimp_code_pushnil (ChimpRef *self)
{
    if (!chimp_code_grow (self)) {
        return CHIMP_FALSE;
    }
    CHIMP_NEXT_INSTR(self) = CHIMP_MAKE_INSTR0(PUSHNIL);
    return CHIMP_TRUE;
}

chimp_bool_t
chimp_code_storename (ChimpRef *self, ChimpRef *id)
{
    int32_t arg;
    if (!chimp_code_grow (self)) {
        return CHIMP_FALSE;
    }

    arg = chimp_code_add_name (self, id);
    if (arg < 0) {
        return CHIMP_FALSE;
    }

    CHIMP_NEXT_INSTR(self) = CHIMP_MAKE_INSTR1(STORENAME, arg);
    return CHIMP_TRUE;
}

chimp_bool_t
chimp_code_getattr (ChimpRef *self, ChimpRef *id)
{
    int32_t arg;
    if (!chimp_code_grow (self)) {
        return CHIMP_FALSE;
    }

    arg = chimp_code_add_name (self, id);
    if (arg < 0) {
        return CHIMP_FALSE;
    }

    CHIMP_NEXT_INSTR(self) = CHIMP_MAKE_INSTR1(GETATTR, arg);
    return CHIMP_TRUE;
}

chimp_bool_t
chimp_code_call (ChimpRef *self)
{
    if (!chimp_code_grow (self)) {
        return CHIMP_FALSE;
    }
    CHIMP_NEXT_INSTR(self) = CHIMP_MAKE_INSTR0(CALL);
    return CHIMP_TRUE;
}

chimp_bool_t
chimp_code_makearray (ChimpRef *self, uint8_t nargs)
{
    if (!chimp_code_grow (self)) {
        return CHIMP_FALSE;
    }
    /* XXX this cast should happen automatically in the make_instr macro */
    CHIMP_NEXT_INSTR(self) = CHIMP_MAKE_INSTR1(MAKEARRAY, (int32_t)nargs);
    return CHIMP_TRUE;
}

chimp_bool_t
chimp_code_makehash (ChimpRef *self, uint8_t nargs)
{
    if (!chimp_code_grow (self)) {
        return CHIMP_FALSE;
    }
    CHIMP_NEXT_INSTR(self) = CHIMP_MAKE_INSTR1(MAKEHASH, (int32_t)nargs);
    return CHIMP_TRUE;
}

chimp_bool_t
chimp_code_jumpiftrue (ChimpRef *self, ChimpLabel *label)
{
    if (label == NULL) {
        chimp_bug (__FILE__, __LINE__, "jump labels can't be null, fool.");
        return CHIMP_FALSE;
    }

    if (!chimp_code_grow (self)) {
        return CHIMP_FALSE;
    }

    *label = CHIMP_CODE(self)->used;

    /* the caller is expected to backpatch this later with a call to
     * chimp_code_patch_jump_location.
     */
    CHIMP_NEXT_INSTR(self) = CHIMP_MAKE_INSTR0(JUMPIFTRUE);

    return CHIMP_TRUE;
}

chimp_bool_t
chimp_code_jumpiffalse (ChimpRef *self, ChimpLabel *label)
{
    if (label == NULL) {
        chimp_bug (__FILE__, __LINE__, "jump labels can't be null, fool.");
        return CHIMP_FALSE;
    }

    if (!chimp_code_grow (self)) {
        return CHIMP_FALSE;
    }

    *label = CHIMP_CODE(self)->used;
    
    /* the caller is expected to backpatch this later with a call to
     * chimp_code_patch_jump_location.
     */
    CHIMP_NEXT_INSTR(self) = CHIMP_MAKE_INSTR0(JUMPIFFALSE);

    return CHIMP_TRUE;
}

chimp_bool_t
chimp_code_jump (ChimpRef *self, ChimpLabel *label)
{
    if (label == NULL) {
        chimp_bug (__FILE__, __LINE__, "jump labels can't be null, fool.");
        return CHIMP_FALSE;
    }

    if (!chimp_code_grow (self)) {
        return CHIMP_FALSE;
    }

    *label = CHIMP_CODE(self)->used;

    CHIMP_NEXT_INSTR(self) = CHIMP_MAKE_INSTR0(JUMP);

    return CHIMP_TRUE;
}

chimp_bool_t
chimp_code_patch_jump_location (ChimpRef *self, ChimpLabel label)
{
    /* XXX limited to this by my current stupid choice of instr repr */
    if (CHIMP_CODE(self)->used > 0x00ffffff) {
        chimp_bug (__FILE__, __LINE__, "attempted to jump to an address > 24 bits");
        return CHIMP_FALSE;
    }

    CHIMP_CODE(self)->bytecode[label] |= (CHIMP_CODE(self)->used & 0x00ffffff);

    return CHIMP_TRUE;
}

chimp_bool_t
chimp_code_eq (ChimpRef *self)
{
    if (!chimp_code_grow (self)) {
        return CHIMP_FALSE;
    }

    CHIMP_NEXT_INSTR(self) = CHIMP_MAKE_INSTR0(CMPEQ);
    return CHIMP_TRUE;
}

chimp_bool_t
chimp_code_neq (ChimpRef *self)
{
    if (!chimp_code_grow (self)) {
        return CHIMP_FALSE;
    }

    CHIMP_NEXT_INSTR(self) = CHIMP_MAKE_INSTR0(CMPNEQ);
    return CHIMP_TRUE;
}

chimp_bool_t
chimp_code_pop (ChimpRef *self)
{
    if (!chimp_code_grow (self)) {
        return CHIMP_FALSE;
    }

    CHIMP_NEXT_INSTR(self) = CHIMP_MAKE_INSTR0(POP);
    return CHIMP_TRUE;
}

static const char *
chimp_code_opcode_str (ChimpOpcode op)
{
    switch (op) {
        case CHIMP_OPCODE_JUMPIFFALSE:
            return "JUMP_IF_FALSE";
        case CHIMP_OPCODE_JUMPIFTRUE:
             return "JUMP_IF_TRUE";
        case CHIMP_OPCODE_JUMP:
             return "JUMP";
        case CHIMP_OPCODE_PUSHCONST:
             return "PUSHCONST";
        case CHIMP_OPCODE_STORENAME:
             return "STORENAME";
        case CHIMP_OPCODE_PUSHNAME:
             return "PUSHNAME";
        case CHIMP_OPCODE_GETATTR:
             return "GETATTR";
        case CHIMP_OPCODE_CALL:
             return "CALL";
        case CHIMP_OPCODE_MAKEARRAY:
             return "MAKEARRAY";
        case CHIMP_OPCODE_MAKEHASH:
             return "MAKEHASH";
        case CHIMP_OPCODE_CMPEQ:
             return "CMP_EQ";
        case CHIMP_OPCODE_CMPNEQ:
             return "CMP_NEQ";
        case CHIMP_OPCODE_POP:
             return "CMP_POP";
        default:
             return "???OPCODE???";
    };
}

ChimpRef *
chimp_code_dump (ChimpRef *self)
{
    /* XXX super duper inefficient & sloppy. */
    /*     mostly for diagnostic purposes atm. */
    size_t i;
    ChimpRef *str = CHIMP_STR_NEW (NULL, "");
    for (i = 0; i < CHIMP_CODE(self)->used; i++) {
        int32_t instr = CHIMP_CODE_INSTR(self, i);
        ChimpOpcode op = ((ChimpOpcode)((instr & 0xff000000) >> 24));
        const char *op_str = chimp_code_opcode_str (op);
        if (!chimp_str_append (str, chimp_int_new (NULL, i))) {
            return NULL;
        }
        if (!chimp_str_append_str (str, " ")) {
            return NULL;
        }
        if (!chimp_str_append_str (str, op_str)) {
            return NULL;
        }
        if (op == CHIMP_OPCODE_PUSHNAME || op == CHIMP_OPCODE_STORENAME || op == CHIMP_OPCODE_GETATTR) {
            if (!chimp_str_append_str (str, " ")) {
                return NULL;
            }
            if (!chimp_str_append (str, CHIMP_INSTR_NAME1(self, i))) {
                return NULL;
            }
        }
        else if (op == CHIMP_OPCODE_PUSHCONST) {
            if (!chimp_str_append_str (str, " ")) {
                return NULL;
            }
            if (!chimp_str_append (str, CHIMP_INSTR_CONST1(self, i))) {
                return NULL;
            }
        }
        else if (op == CHIMP_OPCODE_MAKEARRAY || op == CHIMP_OPCODE_MAKEHASH) {
            if (!chimp_str_append_str (str, " ")) {
                return NULL;
            }
            if (!chimp_str_append (str, chimp_int_new (NULL, CHIMP_INSTR_ARG1(self, i)))) {
                return NULL;
            }
        }
        else if (op == CHIMP_OPCODE_JUMP || op == CHIMP_OPCODE_JUMPIFTRUE || op == CHIMP_OPCODE_JUMPIFFALSE) {
            if (!chimp_str_append_str (str, " ")) {
                return NULL;
            }
            if (!chimp_str_append (str, chimp_int_new (NULL, (instr & 0xffffff)))) {
                return NULL;
            }
        }
        if (!chimp_str_append_str (str, "\n")) {
            return NULL;
        }
    }
    return str;
}

