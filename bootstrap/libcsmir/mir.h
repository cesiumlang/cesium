/* Core MIR: the in-memory form of the core level of the Cesium middle IR
 * (cesiumlang/cesium#8), version 0.
 *
 * A module is built either by the text parser (mir_parse) or directly through
 * the constructors below (the S0 frontend). Names are kept as written; the
 * verifier (mir_verify) resolves them, computes every expression's type and
 * checks the module. Only a verified module may be lowered.
 *
 * Everything is allocated in the module's arena and freed with it.
 */
#ifndef CS_MIR_H
#define CS_MIR_H

#include "cs_common.h"

#define MIR_VERSION 0

/* ---- types ------------------------------------------------------------- */

typedef enum MirTypeKind {
  MT_VOID,
  MT_U8,
  MT_U16,
  MT_U32,
  MT_U64,
  MT_I8,
  MT_I16,
  MT_I32,
  MT_I64,
  MT_UWORD, /* pointer-sized, unsigned; resolved by the target */
  MT_F32,   /* reserved: no operations in v0 */
  MT_F64,   /* reserved: no operations in v0 */
  MT_PTR,
  MT_ARRAY,
  MT_STRUCT
} MirTypeKind;

typedef struct MirType MirType;
typedef struct MirStruct MirStruct;
typedef struct MirExpr MirExpr;
typedef struct MirStmt MirStmt;
typedef struct MirLocal MirLocal;
typedef struct MirGlobal MirGlobal;
typedef struct MirFunc MirFunc;
typedef struct MirModule MirModule;

struct MirType {
  MirTypeKind kind;
  MirType *elem;     /* MT_PTR: pointee; MT_ARRAY: element */
  uint64_t count;    /* MT_ARRAY */
  MirStruct *st;     /* MT_STRUCT */
  MirType *ptr_to;   /* cache: the pointer type to this type */
};

typedef struct MirField {
  const char *name;
  MirType *type;
  uint64_t offset; /* set by layout */
} MirField;

struct MirStruct {
  const char *name;
  MirType type;      /* the MT_STRUCT type naming this struct */
  MirField *fields;
  size_t nfields;
  bool defined;      /* false while only referenced (ptr<Name> before its definition) */
  int line;
  /* layout, computed on demand by the target's rules */
  int layout_state;  /* 0 not started, 1 in progress, 2 done */
  uint64_t size;
  uint64_t align;
  MirStruct *next;
};

/* ---- expressions ------------------------------------------------------- */

typedef enum MirOp {
  ME_CONST,   /* ival : type */
  ME_LOCAL,   /* %name */
  ME_GLOBAL,  /* @name: the address of a global */
  ME_ADDR,    /* addr(%name) */
  ME_LOAD,    /* load(a) */
  ME_INDEX,   /* index(a, b) */
  ME_FIELD,   /* field(a, name) */
  ME_CALL,    /* call @name(args) */
  ME_ADD,
  ME_SUB,
  ME_MUL,
  ME_DIV,
  ME_REM,
  ME_AND,
  ME_OR,
  ME_XOR,
  ME_SHL,
  ME_SHR,
  ME_NOT,
  ME_NEG,
  ME_EQ,
  ME_NE,
  ME_LT,
  ME_LE,
  ME_GT,
  ME_GE,
  ME_LAND,
  ME_LOR,
  ME_LNOT,
  ME_AS,      /* as(targ, a) */
  ME_SIZEOF,  /* sizeof(targ) */
  ME_ALIGNOF, /* alignof(targ) */
  ME_OP_COUNT
} MirOp;

struct MirExpr {
  MirOp op;
  int line;          /* line in the MIR text, or the source line from the frontend; 0 if unknown */
  MirType *type;     /* result type: given for ME_CONST, otherwise set by the verifier */
  MirExpr *a;
  MirExpr *b;
  uint64_t ival;     /* ME_CONST: the value's two's complement bits, truncated to the type */
  const char *name;  /* local, global, callee or field name */
  MirType *targ;     /* ME_AS, ME_SIZEOF, ME_ALIGNOF */
  MirExpr **args;    /* ME_CALL */
  size_t nargs;
  /* set by the verifier */
  MirLocal *local;
  MirGlobal *global;
  MirFunc *callee;
  size_t field_index;
};

/* ---- statements -------------------------------------------------------- */

typedef enum MirStmtKind {
  MS_LOCAL,    /* local %name: type [= a] */
  MS_SET,      /* set %name = a */
  MS_STORE,    /* store a, b */
  MS_EVAL,     /* eval a */
  MS_IF,       /* if a { body } [else { els }] */
  MS_WHILE,    /* while a { body } */
  MS_BREAK,
  MS_CONTINUE,
  MS_RETURN    /* return [a] */
} MirStmtKind;

struct MirLocal {
  const char *name;
  MirType *type;
  bool is_param;
  int slot;          /* unique within the function; set by the verifier */
  MirLocal *next;    /* every local of the function, in declaration order */
};

/* Source lines and columns of a `loc` are below this, QBE's limit. */
#define MIR_LOC_MAX ((uint32_t)1 << 29)

struct MirStmt {
  MirStmtKind kind;
  int line;
  /* the source location, `loc "file" line:col`; loc_line 0 for none */
  const char *loc_file;
  uint32_t loc_line;
  uint32_t loc_col;
  const char *name;  /* MS_LOCAL, MS_SET */
  MirType *type;     /* MS_LOCAL */
  MirExpr *a;
  MirExpr *b;
  MirStmt *body;     /* first statement of the block, or NULL */
  MirStmt *els;
  MirStmt *next;
  MirLocal *local;   /* MS_LOCAL, MS_SET: set by the verifier */
};

/* ---- declarations ------------------------------------------------------ */

typedef enum MirInitKind {
  MI_ZERO,   /* no initializer: all bits zero */
  MI_INT,    /* an integer constant of the global's type */
  MI_BYTES,  /* a byte string, for [N x u8] */
  MI_ADDR    /* the address of another global, for a pointer */
} MirInitKind;

struct MirGlobal {
  const char *name;
  MirType *type;
  bool is_const;
  MirInitKind init;
  uint64_t ival;
  const char *bytes;
  size_t nbytes;
  const char *addr_name;
  int line;
  MirGlobal *next;
};

typedef struct MirParam {
  const char *name; /* NULL for an extern declaration */
  MirType *type;
} MirParam;

struct MirFunc {
  const char *name;
  MirParam *params;
  size_t nparams;
  MirType *ret;
  bool is_extern;    /* declared only; C ABI */
  bool is_export;    /* externally visible; C ABI */
  MirStmt *body;
  int line;
  MirLocal *locals;  /* set by the verifier: parameters first, then every local */
  int nslots;
  MirFunc *next;
};

struct MirModule {
  CsArena arena;
  MirType prim[MT_PTR]; /* the scalar types and void, indexed by kind */
  MirStruct *structs, *structs_tail;
  MirGlobal *globals, *globals_tail;
  MirFunc *funcs, *funcs_tail;
  const char *loc_file; /* the last file name mir_stmt_loc copied */
  bool verified;
  /* the first error, if any */
  bool failed;
  char err[512];
};

/* ---- construction ------------------------------------------------------ */

MirModule *mir_module_new(void);
void mir_module_free(MirModule *m);

/* Records the first error (printf-style); later ones are dropped. */
void mir_error(MirModule *m, int line, const char *fmt, ...);

MirType *mir_prim(MirModule *m, MirTypeKind kind);
MirType *mir_ptr(MirModule *m, MirType *elem);
MirType *mir_array(MirModule *m, MirType *elem, uint64_t count);
/* The struct named `name`, created undefined if it is not known yet. */
MirStruct *mir_struct_ref(MirModule *m, const char *name);
/* Defines a struct; an error if it is already defined. */
MirStruct *mir_struct_define(MirModule *m, const char *name, MirField *fields, size_t nfields, int line);

bool mir_type_eq(const MirType *a, const MirType *b);
bool mir_type_is_int(const MirType *t);
bool mir_type_is_signed(const MirType *t);
bool mir_type_is_scalar(const MirType *t); /* integer or pointer */
/* Appends the type's text form (`u8`, `ptr<Token>`, `[4 x i32]`). */
void mir_type_print(CsBuf *b, const MirType *t);
/* The scalar type named `name` (`u8` ... `uword`, `f32`, `f64`, `void`), or NULL. */
MirType *mir_prim_by_name(MirModule *m, const char *name);
const char *mir_op_name(MirOp op);

MirGlobal *mir_global_add(MirModule *m, const char *name, MirType *type, bool is_const, int line);
MirFunc *mir_func_add(MirModule *m, const char *name, MirParam *params, size_t nparams, MirType *ret, int line);

MirExpr *mir_expr(MirModule *m, MirOp op, int line);
MirExpr *mir_const(MirModule *m, MirType *type, uint64_t value, int line);
MirExpr *mir_local_ref(MirModule *m, const char *name, int line);
MirExpr *mir_global_ref(MirModule *m, const char *name, int line);
MirExpr *mir_unary(MirModule *m, MirOp op, MirExpr *a, int line);
MirExpr *mir_binary(MirModule *m, MirOp op, MirExpr *a, MirExpr *b, int line);
MirExpr *mir_field(MirModule *m, MirExpr *ptr, const char *name, int line);
MirExpr *mir_call(MirModule *m, const char *callee, MirExpr **args, size_t nargs, int line);
MirExpr *mir_as(MirModule *m, MirType *to, MirExpr *a, int line);
MirExpr *mir_type_op(MirModule *m, MirOp op, MirType *t, int line); /* sizeof, alignof */

MirStmt *mir_stmt(MirModule *m, MirStmtKind kind, int line);
/* Gives a statement its source location, copying the file name into the
 * module. A line or column of 0, or of MIR_LOC_MAX or more, or an empty name,
 * leaves the statement without one. */
void mir_stmt_loc(MirModule *m, MirStmt *s, const char *file, uint32_t line, uint32_t col);

/* ---- text -------------------------------------------------------------- */

/* Parses MIR text into a new module. On a syntax error the module is
 * returned with `failed` set and the message in `err`. */
MirModule *mir_parse(const char *text, size_t len);
/* Appends the module's text form. Printing a parsed module and parsing the
 * result gives the same module. */
void mir_print(CsBuf *out, const MirModule *m);

/* ---- verification ------------------------------------------------------ */

/* Resolves names, types every expression and checks the module. Returns
 * false with the first error in m->err. */
bool mir_verify(MirModule *m);

/* ---- targets and layout ------------------------------------------------ */

typedef enum MirObjFormat { MIR_ELF, MIR_MACHO, MIR_COFF } MirObjFormat;

typedef struct MirTarget {
  const char *name;      /* the QBE target name */
  uint64_t ptr_size;     /* bytes; also the size of uword */
  MirObjFormat format;   /* of the objects its assembly becomes */
} MirTarget;

const MirTarget *mir_target_by_name(const char *name);
/* The QBE target for the machine this was compiled for, or NULL. */
const MirTarget *mir_target_host(void);

/* Size and alignment in bytes by the target's C layout rules. A struct's
 * field offsets are filled in as a side effect. */
uint64_t mir_sizeof(const MirTarget *t, MirType *type);
uint64_t mir_alignof(const MirTarget *t, MirType *type);

/* ---- lowering ---------------------------------------------------------- */

/* Appends QBE IL for a verified module. Returns the file of its first
 * dbgfile, or NULL if it has no locations. */
const char *mir_lower_qbe(CsBuf *out, MirModule *m, const MirTarget *t);

/* ---- debug information ------------------------------------------------- */

/* Appends s as an assembler string, quoted: \\, \" and \ooo. */
void mir_asm_string(CsBuf *out, const char *s);

/* Whether the target's objects get a DWARF compile unit: ELF and Mach-O. */
bool mir_dwarf_target(const MirTarget *t);
/* The assembly that goes before QBE's: a label at the start of the text. */
void mir_dwarf_head(CsBuf *out, const MirTarget *t);
/* The assembly that goes after QBE's: a label at the end of the text and a
 * compile unit covering it, named file, whose line table is the one the
 * assembler writes for QBE's .file and .loc. */
void mir_dwarf_tail(CsBuf *out, const MirTarget *t, const char *file);

#endif
