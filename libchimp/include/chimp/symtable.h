#ifndef _CHIMP_SYMTABLE_H_INCLUDED_
#define _CHIMP_SYMTABLE_H_INCLUDED_

#include <chimp/any.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _ChimpSymtable {
    ChimpAny base;
    ChimpRef *filename;
    ChimpRef *lookup;
    ChimpRef *stack;
    ChimpRef *current;
} ChimpSymtable;

typedef struct _ChimpSymtableEntry {
    ChimpAny  base;
    ChimpRef *symtable;
    ChimpRef *scope;
    ChimpRef *symbols;
    ChimpRef *varnames;
    ChimpRef *parent;
    ChimpRef *children;
} ChimpSymtableEntry;

chimp_bool_t
chimp_symtable_class_bootstrap (void);

chimp_bool_t
chimp_symtable_entry_class_bootstrap (void);

ChimpRef *
chimp_symtable_new_from_ast (ChimpRef *filename, ChimpRef *ast);

#define CHIMP_SYMTABLE(ref) \
    CHIMP_CHECK_CAST(ChimpSymtable, (ref), CHIMP_VALUE_TYPE_SYMTABLE)

#define CHIMP_SYMTABLE_ENTRY(ref) \
    CHIMP_CHECK_CAST( \
        ChimpSymtableEntry, (ref), CHIMP_VALUE_TYPE_SYMTABLE_ENTRY)


CHIMP_EXTERN_CLASS(symtable);
CHIMP_EXTERN_CLASS(symtable_entry);

#ifdef __cplusplus
};
#endif

#endif

