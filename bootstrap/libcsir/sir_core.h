/* Core SIR: the in-memory form of the core level of the Cesium semantic IR
 * (cesiumlang/cesium#8), version 0.
 *
 * A module is built either by the text parser (sir_parse) or directly through
 * the constructors below (the S0 frontend). Names are kept as written; the
 * verifier (sir_verify) resolves them, computes every expression's type and
 * checks the module. Only a verified module may be lowered.
 *
 * Everything is allocated in the module's arena and freed with it.
 */
#ifndef CS_SIR_CORE_H
#define CS_SIR_CORE_H

#include "cs_common.h"

#define SIR_VERSION 0

/* ---- types ------------------------------------------------------------- */

typedef enum SirTypeKind {
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
} SirTypeKind;

typedef struct SirType SirType;
typedef struct SirStruct SirStruct;
typedef struct SirExpr SirExpr;
typedef struct SirStmt SirStmt;
typedef struct SirLocal SirLocal;
typedef struct SirGlobal SirGlobal;
typedef struct SirFunc SirFunc;
typedef struct SirModule SirModule;

struct SirType {
  SirTypeKind kind;
  SirType *elem;     /* MT_PTR: pointee; MT_ARRAY: element */
  uint64_t count;    /* MT_ARRAY */
  SirStruct *st;     /* MT_STRUCT */
  SirType *ptr_to;   /* cache: the pointer type to this type */
};

typedef struct SirField {
  const char *name;
  SirType *type;
  uint64_t offset; /* set by layout */
} SirField;

struct SirStruct {
  const char *name;
  SirType type;      /* the MT_STRUCT type naming this struct */
  SirField *fields;
  size_t nfields;
  bool defined;      /* false while only referenced (ptr<Name> before its definition) */
  int line;
  /* layout, computed on demand by the target's rules */
  int layout_state;  /* 0 not started, 1 in progress, 2 done */
  uint64_t size;
  uint64_t align;
  SirStruct *next;
};

/* ---- expressions ------------------------------------------------------- */

typedef enum SirOp {
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
} SirOp;

struct SirExpr {
  SirOp op;
  int line;          /* line in the SIR text, or the source line from the frontend; 0 if unknown */
  SirType *type;     /* result type: given for ME_CONST, otherwise set by the verifier */
  SirExpr *a;
  SirExpr *b;
  uint64_t ival;     /* ME_CONST: the value's two's complement bits, truncated to the type */
  const char *name;  /* local, global, callee or field name */
  SirType *targ;     /* ME_AS, ME_SIZEOF, ME_ALIGNOF */
  SirExpr **args;    /* ME_CALL */
  size_t nargs;
  /* set by the verifier */
  SirLocal *local;
  SirGlobal *global;
  SirFunc *callee;
  size_t field_index;
};

/* ---- statements -------------------------------------------------------- */

typedef enum SirStmtKind {
  MS_LOCAL,    /* local %name: type [= a] */
  MS_SET,      /* set %name = a */
  MS_STORE,    /* store a, b */
  MS_EVAL,     /* eval a */
  MS_IF,       /* if a { body } [else { els }] */
  MS_WHILE,    /* while a { body } */
  MS_BREAK,
  MS_CONTINUE,
  MS_RETURN    /* return [a] */
} SirStmtKind;

struct SirLocal {
  const char *name;
  SirType *type;
  bool is_param;
  int slot;          /* unique within the function; set by the verifier */
  SirLocal *next;    /* every local of the function, in declaration order */
};

/* Source lines and columns of a `loc` are below this, QBE's limit. */
#define SIR_LOC_MAX ((uint32_t)1 << 29)

struct SirStmt {
  SirStmtKind kind;
  int line;
  /* the source location, `loc "file" line:col`; loc_line 0 for none */
  const char *loc_file;
  uint32_t loc_line;
  uint32_t loc_col;
  const char *name;  /* MS_LOCAL, MS_SET */
  SirType *type;     /* MS_LOCAL */
  SirExpr *a;
  SirExpr *b;
  SirStmt *body;     /* first statement of the block, or NULL */
  SirStmt *els;
  SirStmt *next;
  SirLocal *local;   /* MS_LOCAL, MS_SET: set by the verifier */
};

/* ---- declarations ------------------------------------------------------ */

typedef enum SirInitKind {
  MI_ZERO,   /* no initializer: all bits zero */
  MI_INT,    /* an integer constant of the global's type */
  MI_BYTES,  /* a byte string, for [N x u8] */
  MI_ADDR    /* the address of another global, for a pointer */
} SirInitKind;

struct SirGlobal {
  const char *name;
  SirType *type;
  bool is_const;
  SirInitKind init;
  uint64_t ival;
  const char *bytes;
  size_t nbytes;
  const char *addr_name;
  int line;
  SirGlobal *next;
};

typedef struct SirParam {
  const char *name; /* NULL for an extern declaration */
  SirType *type;
} SirParam;

struct SirFunc {
  const char *name;
  SirParam *params;
  size_t nparams;
  SirType *ret;
  bool is_extern;    /* declared only; C ABI */
  bool is_export;    /* externally visible; C ABI */
  SirStmt *body;
  int line;
  SirLocal *locals;  /* set by the verifier: parameters first, then every local */
  int nslots;
  SirFunc *next;
};

struct SirModule {
  CsArena arena;
  SirType prim[MT_PTR]; /* the scalar types and void, indexed by kind */
  SirStruct *structs, *structs_tail;
  SirGlobal *globals, *globals_tail;
  SirFunc *funcs, *funcs_tail;
  const char *loc_file; /* the last file name sir_stmt_loc copied */
  bool verified;
  /* the first error, if any, and its code (bootstrap/diagnostics.md) */
  bool failed;
  char err[512];
  const char *err_code;
};

/* ---- construction ------------------------------------------------------ */

SirModule *sir_module_new(void);
void sir_module_free(SirModule *m);

/* Records the first error (printf-style), with its code, a literal such as
 * "E1101"; later ones are dropped. */
void sir_error(SirModule *m, int line, const char *code, const char *fmt, ...);

SirType *sir_prim(SirModule *m, SirTypeKind kind);
SirType *sir_ptr(SirModule *m, SirType *elem);
SirType *sir_array(SirModule *m, SirType *elem, uint64_t count);
/* The struct named `name`, created undefined if it is not known yet. */
SirStruct *sir_struct_ref(SirModule *m, const char *name);
/* Defines a struct; an error if it is already defined. */
SirStruct *sir_struct_define(SirModule *m, const char *name, SirField *fields, size_t nfields, int line);

bool sir_type_eq(const SirType *a, const SirType *b);
bool sir_type_is_int(const SirType *t);
bool sir_type_is_signed(const SirType *t);
bool sir_type_is_scalar(const SirType *t); /* integer or pointer */
/* Appends the type's text form (`u8`, `ptr<Token>`, `[4 x i32]`). */
void sir_type_print(CsBuf *b, const SirType *t);
/* The scalar type named `name` (`u8` ... `uword`, `f32`, `f64`, `void`), or NULL. */
SirType *sir_prim_by_name(SirModule *m, const char *name);
const char *sir_op_name(SirOp op);

SirGlobal *sir_global_add(SirModule *m, const char *name, SirType *type, bool is_const, int line);
SirFunc *sir_func_add(SirModule *m, const char *name, SirParam *params, size_t nparams, SirType *ret, int line);

SirExpr *sir_expr(SirModule *m, SirOp op, int line);
SirExpr *sir_const(SirModule *m, SirType *type, uint64_t value, int line);
SirExpr *sir_local_ref(SirModule *m, const char *name, int line);
SirExpr *sir_global_ref(SirModule *m, const char *name, int line);
SirExpr *sir_unary(SirModule *m, SirOp op, SirExpr *a, int line);
SirExpr *sir_binary(SirModule *m, SirOp op, SirExpr *a, SirExpr *b, int line);
SirExpr *sir_field(SirModule *m, SirExpr *ptr, const char *name, int line);
SirExpr *sir_call(SirModule *m, const char *callee, SirExpr **args, size_t nargs, int line);
SirExpr *sir_as(SirModule *m, SirType *to, SirExpr *a, int line);
SirExpr *sir_type_op(SirModule *m, SirOp op, SirType *t, int line); /* sizeof, alignof */

SirStmt *sir_stmt(SirModule *m, SirStmtKind kind, int line);
/* Gives a statement its source location, copying the file name into the
 * module. A line or column of 0, or of SIR_LOC_MAX or more, or an empty name,
 * leaves the statement without one. */
void sir_stmt_loc(SirModule *m, SirStmt *s, const char *file, uint32_t line, uint32_t col);

/* ---- text -------------------------------------------------------------- */

/* Parses SIR text into a new module. On a syntax error the module is
 * returned with `failed` set and the message in `err`. */
SirModule *sir_parse(const char *text, size_t len);
/* Appends the module's text form. Printing a parsed module and parsing the
 * result gives the same module. */
void sir_print(CsBuf *out, const SirModule *m);

/* ---- verification ------------------------------------------------------ */

/* Resolves names, types every expression and checks the module. Returns
 * false with the first error in m->err. */
bool sir_verify(SirModule *m);

/* ---- targets and layout ------------------------------------------------ */

typedef enum SirObjFormat { SIR_ELF, SIR_MACHO, SIR_COFF } SirObjFormat;

typedef struct SirTarget {
  const char *name;      /* the QBE target name */
  uint64_t ptr_size;     /* bytes; also the size of uword */
  SirObjFormat format;   /* of the objects its assembly becomes */
} SirTarget;

const SirTarget *sir_target_by_name(const char *name);
/* The QBE target for the machine this was compiled for, or NULL. */
const SirTarget *sir_target_host(void);

/* Size and alignment in bytes by the target's C layout rules. A struct's
 * field offsets are filled in as a side effect. */
uint64_t sir_sizeof(const SirTarget *t, SirType *type);
uint64_t sir_alignof(const SirTarget *t, SirType *type);

/* ---- lowering ---------------------------------------------------------- */

/* Appends QBE IL for a verified module; with variables, each local kept in
 * memory for debuggers, with markers for the rewrite of the assembly
 * (sir_debug.c). Returns the file of its first dbgfile, or NULL if it has no
 * locations. */
const char *sir_lower_qbe(CsBuf *out, SirModule *m, const SirTarget *t, bool variables);

/* ---- variables for debuggers (sir_debug.c) ----------------------------- */

/* A function's lexical scopes and variables in a debug build. Scope 0 is the
 * function; every other block that declares a local is a scope, numbered in
 * the order the lowering meets them. */
typedef struct SirDebugFunc {
  const SirFunc *f;
  size_t nscopes;
  int *parent;       /* of each scope; -1 for scope 0 */
  size_t nlocals;    /* f's locals, in their order */
  SirLocal **locals;
  int *scope;        /* of each local */
} SirDebugFunc;

/* Whether a block (its first statement) declares a local of its own. */
bool sir_block_is_scope(const SirStmt *body);
/* Fills d for f, in a's memory. */
void sir_debug_func(CsArena *a, SirDebugFunc *d, const SirFunc *f);
/* Appends the symbol of a marker, without QBE's $: kind 'v' for local n's
 * address, 'o' and 'c' where scope n opens and closes, 't' in the block of a
 * trap inside scope n; func is the index of the function among the module's
 * defined ones. */
void sir_debug_marker(CsBuf *out, char kind, int func, int n);
/* Reads a marker from its symbol in an instruction: its kind and number, or
 * kind 0. */
char sir_debug_find_marker(const char *line, size_t len, int *n);

/* Where a local is: at an offset from the frame pointer (rbp, x29) or the
 * stack pointer, or not found. */
typedef enum SirBase { SIR_BASE_NONE, SIR_BASE_FP, SIR_BASE_SP } SirBase;
typedef struct SirDebugVar {
  SirBase base;
  int64_t offset;
} SirDebugVar;

/* AArch64's 32 registers, then x86-64's 16 */
#define SIR_SCAN_REGS 48

/* The reading of one function's markers (sir_debug_scan.c). */
typedef struct SirDebugScan {
  SirDebugVar *vars;
  size_t nlocals;
  SirBase slot_base[SIR_SCAN_REGS]; /* a register holding a slot's address */
  int64_t slot_off[SIR_SCAN_REGS];
  bool imm_known[SIR_SCAN_REGS];    /* a register holding a constant */
  int64_t imm[SIR_SCAN_REGS];
  int pending[SIR_SCAN_REGS];       /* a register holding local n's marker */
} SirDebugScan;

/* Where a reading of QBE's assembly is: in the text, after a directive. */
typedef struct SirAsmLines {
  bool in_text;
  bool was_directive;
} SirAsmLines;

/* Reads a line of QBE's assembly, without its line ending, and returns
 * whether it is a function's label: in the text, right after the
 * function's directives. Call it for every line. */
bool sir_asm_func_label(SirAsmLines *s, const char *line, size_t len);

/* Starts reading a function whose nlocals locals go into vars. */
void sir_debug_scan_start(SirDebugScan *s, SirDebugVar *vars, size_t nlocals);
/* Reads an instruction. Returns 'o', 'c' or 't', with the scope in *n, if a
 * scope's marker starts on it; 0 otherwise. */
char sir_debug_scan_line(SirDebugScan *s, const char *line, size_t len, int *n);
/* Whether a line is the instruction QBE writes for hlt: ud2, brk. */
bool sir_asm_is_trap(const char *line, size_t len);

/* ---- debug information ------------------------------------------------- */

/* Appends s as an assembler string, quoted: \\, \" and \ooo. */
void sir_asm_string(CsBuf *out, const char *s);

/* Whether the target's objects get a DWARF compile unit: ELF and Mach-O. */
bool sir_dwarf_target(const SirTarget *t);
/* The assembly that goes before QBE's: a label at the start of the text. */
void sir_dwarf_head(CsBuf *out, const SirTarget *t);
/* What variables add to the compile unit (sir_dwarf_vars.c): its children,
 * abbreviations from code 2 on, and strings for .debug_str. */
typedef struct SirDwarfExtra {
  CsBuf info;
  CsBuf abbrev;
  CsBuf str;
} SirDwarfExtra;

/* The assembly that goes after QBE's: a label at the end of the text and a
 * compile unit covering it, named file, whose line table is the one the
 * assembler writes for QBE's .file and .loc; with extra, its children. */
void sir_dwarf_tail(CsBuf *out, const SirTarget *t, const char *file, const SirDwarfExtra *extra);
/* QBE's assembly with a compile unit around it, as sir_dwarf_head and
 * sir_dwarf_tail write it, and the variables of m's functions in it, read
 * from their markers (cesiumlang/cesium#143). */
void sir_dwarf_rewrite(CsBuf *out, const char *in, size_t len, const SirModule *m, const SirTarget *t,
                       const char *file);

/* Whether the target's objects get CodeView line tables instead: COFF. */
bool sir_codeview_target(const SirTarget *t);
/* Appends file as an absolute path with backslashes, without . and ..
 * components; relative to the current directory if it is relative. */
void sir_codeview_path(CsBuf *out, const char *file);
/* Appends QBE's assembly with its .file and .loc made CodeView directives,
 * and the .debug$S section of their line tables; with m, the variables of
 * its functions and their types too, for target t. */
void sir_codeview_rewrite(CsBuf *out, const char *in, size_t len, const SirModule *m, const SirTarget *t);
/* A function's variables as a rewrite of QBE's assembly found them: its
 * number among the functions read, its scopes and locals, where each local
 * is, and which scopes have labels (PREFIXcs_sbID_K and PREFIXcs_seID_K).
 * QBE moves a trap's block out of its scope, so each trap in a scope has
 * labels of its own (PREFIXcs_tbID_J and PREFIXcs_teID_J) and its scope's
 * variables. */
typedef struct SirVarsFunc {
  uint32_t id;
  SirDebugFunc d;
  SirDebugVar *vars;
  bool *opened;
  bool *closed;
  CsBuf traps;    /* the scope of each trap, an int */
  int trap_state; /* 1 in a trap's block, 2 after its instruction */
} SirVarsFunc;

/* The reading of a module's markers by a rewrite (sir_debug.c). */
typedef struct SirVars {
  const SirModule *m;
  const char *prefix; /* of the assembler's local labels */
  bool underscore;    /* symbols start with _ (Mach-O) */
  CsArena arena;
  CsBuf funcs;        /* a SirVarsFunc pointer, or NULL, per function kept */
  SirVarsFunc *cur;   /* the function being read, if it has variables */
  SirDebugScan scan;
  uint32_t count;
} SirVars;

/* Starts reading m's markers in assembly for t; m may be NULL, for none. */
void sir_vars_init(SirVars *v, const SirModule *m, const SirTarget *t);
void sir_vars_free(SirVars *v);
/* A function's label, as QBE writes it. */
void sir_vars_begin(SirVars *v, const char *label);
/* Reads an instruction; a scope's first open marker, and its first close
 * marker after that, get its labels, appended to dst before the line. */
void sir_vars_line(SirVars *v, CsBuf *dst, const char *line, size_t len);
/* A function's end, where its scopes still open close, their labels
 * appended to out; with keep, the function (or NULL) joins v->funcs.
 * Returns it, or NULL. */
SirVarsFunc *sir_vars_end(SirVars *v, CsBuf *out, bool keep);
/* The number of f's traps that have labels. */
size_t sir_vars_ntraps(const SirVarsFunc *f);
/* Fills chain, with room for f->d.nscopes, with the scopes around trap j,
 * from the outermost one inside the function to the trap's own. Returns the
 * chain's length. */
size_t sir_vars_chain(const SirVarsFunc *f, size_t j, int *chain);

/* The .debug$T records of one object (sir_codeview_vars.c). */
typedef struct SirCvTypes {
  CsBuf map;      /* the types given an index so far */
  CsBuf records;
  uint32_t next;  /* the next index */
} SirCvTypes;

void sir_cv_types_init(SirCvTypes *types);
void sir_cv_types_free(SirCvTypes *types);
/* Appends the S_REGREL32 and S_BLOCK32 records of f, within its S_GPROC32,
 * adding their types to types. */
void sir_cv_put_vars(CsBuf *out, SirCvTypes *types, const SirVarsFunc *f, const SirTarget *t);
/* Appends the .debug$T section, if there are types. */
void sir_cv_put_types(CsBuf *out, const SirCvTypes *types);

/* Appends body, the code of function name from its label, with .seh_proc and
 * its prologue described in .seh_ directives; .seh_endproc is the caller's. */
void sir_seh_prologue(CsBuf *out, const char *name, const char *body, size_t len);

#endif
