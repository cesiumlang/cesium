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

/* Appends QBE IL for a verified module; with variables, each local kept in
 * memory for debuggers, with markers for the rewrite of the assembly
 * (mir_debug.c). Returns the file of its first dbgfile, or NULL if it has no
 * locations. */
const char *mir_lower_qbe(CsBuf *out, MirModule *m, const MirTarget *t, bool variables);

/* ---- variables for debuggers (mir_debug.c) ----------------------------- */

/* A function's lexical scopes and variables in a debug build. Scope 0 is the
 * function; every other block that declares a local is a scope, numbered in
 * the order the lowering meets them. */
typedef struct MirDebugFunc {
  const MirFunc *f;
  size_t nscopes;
  int *parent;       /* of each scope; -1 for scope 0 */
  size_t nlocals;    /* f's locals, in their order */
  MirLocal **locals;
  int *scope;        /* of each local */
} MirDebugFunc;

/* Whether a block (its first statement) declares a local of its own. */
bool mir_block_is_scope(const MirStmt *body);
/* Fills d for f, in a's memory. */
void mir_debug_func(CsArena *a, MirDebugFunc *d, const MirFunc *f);
/* Appends the symbol of a marker, without QBE's $: kind 'v' for local n's
 * address, 'o' and 'c' where scope n opens and closes, 't' in the block of a
 * trap inside scope n; func is the index of the function among the module's
 * defined ones. */
void mir_debug_marker(CsBuf *out, char kind, int func, int n);
/* Reads a marker from its symbol in an instruction: its kind and number, or
 * kind 0. */
char mir_debug_find_marker(const char *line, size_t len, int *n);

/* Where a local is: at an offset from the frame pointer (rbp, x29) or the
 * stack pointer, or not found. */
typedef enum MirBase { MIR_BASE_NONE, MIR_BASE_FP, MIR_BASE_SP } MirBase;
typedef struct MirDebugVar {
  MirBase base;
  int64_t offset;
} MirDebugVar;

/* AArch64's 32 registers, then x86-64's 16 */
#define MIR_SCAN_REGS 48

/* The reading of one function's markers (mir_debug_scan.c). */
typedef struct MirDebugScan {
  MirDebugVar *vars;
  size_t nlocals;
  MirBase slot_base[MIR_SCAN_REGS]; /* a register holding a slot's address */
  int64_t slot_off[MIR_SCAN_REGS];
  bool imm_known[MIR_SCAN_REGS];    /* a register holding a constant */
  int64_t imm[MIR_SCAN_REGS];
  int pending[MIR_SCAN_REGS];       /* a register holding local n's marker */
} MirDebugScan;

/* Where a reading of QBE's assembly is: in the text, after a directive. */
typedef struct MirAsmLines {
  bool in_text;
  bool was_directive;
} MirAsmLines;

/* Reads a line of QBE's assembly, without its line ending, and returns
 * whether it is a function's label: in the text, right after the
 * function's directives. Call it for every line. */
bool mir_asm_func_label(MirAsmLines *s, const char *line, size_t len);

/* Starts reading a function whose nlocals locals go into vars. */
void mir_debug_scan_start(MirDebugScan *s, MirDebugVar *vars, size_t nlocals);
/* Reads an instruction. Returns 'o', 'c' or 't', with the scope in *n, if a
 * scope's marker starts on it; 0 otherwise. */
char mir_debug_scan_line(MirDebugScan *s, const char *line, size_t len, int *n);
/* Whether a line is the instruction QBE writes for hlt: ud2, brk. */
bool mir_asm_is_trap(const char *line, size_t len);

/* ---- debug information ------------------------------------------------- */

/* Appends s as an assembler string, quoted: \\, \" and \ooo. */
void mir_asm_string(CsBuf *out, const char *s);

/* Whether the target's objects get a DWARF compile unit: ELF and Mach-O. */
bool mir_dwarf_target(const MirTarget *t);
/* The assembly that goes before QBE's: a label at the start of the text. */
void mir_dwarf_head(CsBuf *out, const MirTarget *t);
/* What variables add to the compile unit (mir_dwarf_vars.c): its children,
 * abbreviations from code 2 on, and strings for .debug_str. */
typedef struct MirDwarfExtra {
  CsBuf info;
  CsBuf abbrev;
  CsBuf str;
} MirDwarfExtra;

/* The assembly that goes after QBE's: a label at the end of the text and a
 * compile unit covering it, named file, whose line table is the one the
 * assembler writes for QBE's .file and .loc; with extra, its children. */
void mir_dwarf_tail(CsBuf *out, const MirTarget *t, const char *file, const MirDwarfExtra *extra);
/* QBE's assembly with a compile unit around it, as mir_dwarf_head and
 * mir_dwarf_tail write it, and the variables of m's functions in it, read
 * from their markers (cesiumlang/cesium#143). */
void mir_dwarf_rewrite(CsBuf *out, const char *in, size_t len, const MirModule *m, const MirTarget *t,
                       const char *file);

/* Whether the target's objects get CodeView line tables instead: COFF. */
bool mir_codeview_target(const MirTarget *t);
/* Appends file as an absolute path with backslashes, without . and ..
 * components; relative to the current directory if it is relative. */
void mir_codeview_path(CsBuf *out, const char *file);
/* Appends QBE's assembly with its .file and .loc made CodeView directives,
 * and the .debug$S section of their line tables; with m, the variables of
 * its functions and their types too, for target t. */
void mir_codeview_rewrite(CsBuf *out, const char *in, size_t len, const MirModule *m, const MirTarget *t);
/* A function's variables as a rewrite of QBE's assembly found them: its
 * number among the functions read, its scopes and locals, where each local
 * is, and which scopes have labels (PREFIXcs_sbID_K and PREFIXcs_seID_K).
 * QBE moves a trap's block out of its scope, so each trap in a scope has
 * labels of its own (PREFIXcs_tbID_J and PREFIXcs_teID_J) and its scope's
 * variables. */
typedef struct MirVarsFunc {
  uint32_t id;
  MirDebugFunc d;
  MirDebugVar *vars;
  bool *opened;
  bool *closed;
  CsBuf traps;    /* the scope of each trap, an int */
  int trap_state; /* 1 in a trap's block, 2 after its instruction */
} MirVarsFunc;

/* The reading of a module's markers by a rewrite (mir_debug.c). */
typedef struct MirVars {
  const MirModule *m;
  const char *prefix; /* of the assembler's local labels */
  bool underscore;    /* symbols start with _ (Mach-O) */
  CsArena arena;
  CsBuf funcs;        /* a MirVarsFunc pointer, or NULL, per function kept */
  MirVarsFunc *cur;   /* the function being read, if it has variables */
  MirDebugScan scan;
  uint32_t count;
} MirVars;

/* Starts reading m's markers in assembly for t; m may be NULL, for none. */
void mir_vars_init(MirVars *v, const MirModule *m, const MirTarget *t);
void mir_vars_free(MirVars *v);
/* A function's label, as QBE writes it. */
void mir_vars_begin(MirVars *v, const char *label);
/* Reads an instruction; a scope's first open marker, and its first close
 * marker after that, get its labels, appended to dst before the line. */
void mir_vars_line(MirVars *v, CsBuf *dst, const char *line, size_t len);
/* A function's end, where its scopes still open close, their labels
 * appended to out; with keep, the function (or NULL) joins v->funcs.
 * Returns it, or NULL. */
MirVarsFunc *mir_vars_end(MirVars *v, CsBuf *out, bool keep);
/* The number of f's traps that have labels. */
size_t mir_vars_ntraps(const MirVarsFunc *f);
/* Fills chain, with room for f->d.nscopes, with the scopes around trap j,
 * from the outermost one inside the function to the trap's own. Returns the
 * chain's length. */
size_t mir_vars_chain(const MirVarsFunc *f, size_t j, int *chain);

/* The .debug$T records of one object (mir_codeview_vars.c). */
typedef struct MirCvTypes {
  CsBuf map;      /* the types given an index so far */
  CsBuf records;
  uint32_t next;  /* the next index */
} MirCvTypes;

void mir_cv_types_init(MirCvTypes *types);
void mir_cv_types_free(MirCvTypes *types);
/* Appends the S_REGREL32 and S_BLOCK32 records of f, within its S_GPROC32,
 * adding their types to types. */
void mir_cv_put_vars(CsBuf *out, MirCvTypes *types, const MirVarsFunc *f, const MirTarget *t);
/* Appends the .debug$T section, if there are types. */
void mir_cv_put_types(CsBuf *out, const MirCvTypes *types);

/* Appends body, the code of function name from its label, with .seh_proc and
 * its prologue described in .seh_ directives; .seh_endproc is the caller's. */
void mir_seh_prologue(CsBuf *out, const char *name, const char *body, size_t len);

#endif
