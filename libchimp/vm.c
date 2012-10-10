#include "chimp/vm.h"
#include "chimp/array.h"
#include "chimp/code.h"
#include "chimp/object.h"
#include "chimp/task.h"

struct _ChimpVM {
    ChimpRef  *stack;
};

ChimpVM *
chimp_vm_new (void)
{
    ChimpVM *vm = CHIMP_MALLOC(ChimpVM, sizeof(*vm));
    if (vm == NULL) {
        return NULL;
    }
    vm->stack = chimp_array_new (NULL);
    return vm;
}

void
chimp_vm_delete (ChimpVM *vm)
{
    CHIMP_FREE(vm);
}

static ChimpRef *
chimp_vm_pop (ChimpVM *vm)
{
    return chimp_array_pop (vm->stack);
}

static chimp_bool_t
chimp_vm_push (ChimpVM *vm, ChimpRef *value)
{
    return chimp_array_push (vm->stack, value);
}

static ChimpRef *
chimp_vm_eval_frame (ChimpVM *vm, ChimpRef *frame)
{
    ChimpRef *code = CHIMP_FRAME(frame)->code;
    ChimpRef *locals = CHIMP_FRAME(frame)->locals;

    size_t pc = 0;
    while (pc < CHIMP_CODE_SIZE(code)) {
        switch (CHIMP_INSTR_OP(code, pc)) {
            case CHIMP_OPCODE_PUSHCONST:
            {
                ChimpRef *value = CHIMP_INSTR_CONST1(code, pc);
                if (value == NULL) {
                    chimp_bug (__FILE__, __LINE__, "unknown or missing const at pc=%d", pc);
                    return NULL;
                }
                if (!chimp_vm_push(vm, value)) {
                    return NULL;
                }
                pc++;
                break;
            }
            case CHIMP_OPCODE_PUSHNAME:
            {
                ChimpRef *value;
                ChimpRef *name = CHIMP_INSTR_NAME1(code, pc);
                if (name == NULL) {
                    chimp_bug (__FILE__, __LINE__, "unknown or missing const at pc=%d", pc);
                    return NULL;
                }
                /* TODO find value in the current namespace */
                value = chimp_hash_get (locals, name);
                if (value == NULL || value == chimp_nil) {
                    chimp_bug (__FILE__, __LINE__, "no such local: %s", CHIMP_STR_DATA(name));
                    return NULL;
                }
                if (!chimp_vm_push (vm, value)) {
                    return NULL;
                }
                pc++;
                break;
            }
            case CHIMP_OPCODE_GETATTR:
            {
                ChimpRef *attr;
                ChimpRef *target;
                ChimpRef *result;
                
                attr = chimp_vm_pop (vm);
                if (attr == NULL) {
                    return NULL;
                }
                target = chimp_vm_pop (vm);
                if (target == NULL) {
                    return NULL;
                }
                result = chimp_object_getattr (target, attr);
                if (result == NULL) {
                    return NULL;
                }
                if (!chimp_vm_push (vm, result)) {
                    return NULL;
                }
                pc++;
                break;
            }
            case CHIMP_OPCODE_CALL:
            {
                ChimpRef *args;
                ChimpRef *target;
                ChimpRef *result;

                args = chimp_vm_pop (vm);
                if (args == NULL) {
                    return NULL;
                }
                target = chimp_vm_pop (vm);
                if (target == NULL) {
                    return NULL;
                }
                result = chimp_object_call (target, args);
                if (result == NULL) {
                    chimp_bug (__FILE__, __LINE__, "target is not callable");
                    return NULL;
                }
                if (!chimp_vm_push (vm, result)) {
                    return NULL;
                }
                pc++;
                break;
            }
            case CHIMP_OPCODE_MAKEARRAY:
            {
                ChimpRef *array;
                int32_t nargs = CHIMP_INSTR_ARG1(code, pc);
                int32_t i;

                array = chimp_array_new (NULL);
                if (array == NULL) {
                    return NULL;
                }
                for (i = 0; i < nargs; i++) {
                    ChimpRef *value = chimp_vm_pop (vm);
                    if (value == NULL) {
                        return NULL;
                    }
                    chimp_array_push (array, value);
                }
                if (!chimp_vm_push (vm, array)) {
                    return NULL;
                }

                pc++;
                break;
            }
            default:
            {
                chimp_bug (__FILE__, __LINE__, "unknown opcode: %d", CHIMP_INSTR_OP(code, pc));
                return NULL;
            }
        };
    }

    return chimp_vm_pop(vm);
}

ChimpRef *
chimp_vm_eval (ChimpVM *vm, ChimpRef *code, ChimpRef *locals)
{
    ChimpRef *frame;
    if (vm == NULL) {
        vm = CHIMP_CURRENT_VM;
    }
    if (code == NULL) {
        chimp_bug (__FILE__, __LINE__, "NULL code object passed to chimp_vm_eval");
        return NULL;
    }
    frame = chimp_frame_new (NULL, code, locals);
    if (frame == NULL) {
        return NULL;
    }
    return chimp_vm_eval_frame (vm, frame);
}

