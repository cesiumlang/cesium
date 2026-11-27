/* Core SIR: module construction, types, targets and layout. */
#include "sir_core.h"

/* ---- module ------------------------------------------------------------ */

SirModule *sir_module_new(void) {
  CsArena arena = {NULL};
  SirModule *m = (SirModule *)cs_arena_new(&arena, sizeof(SirModule));
  int k;
  m->arena = arena;
  for (k = 0; k < MT_PTR; k++) {
    m->prim[k].kind = (SirTypeKind)k;
  }
  return m;
}

void sir_module_free(SirModule *m) {
  if (m != NULL) {
    /* the module itself lives in its arena */
    CsArena arena = m->arena;
    cs_arena_free(&arena);
  }
}

void sir_error(SirModule *m, int line, const char *code, const char *fmt, ...) {
  va_list ap;
  int n = 0;
  if (m->failed) {
    return;
  }
  m->failed = true;
  m->err_code = code;
  if (line > 0) {
    n = snprintf(m->err, sizeof m->err, "line %d: ", line);
    if (n < 0 || (size_t)n >= sizeof m->err) {
      n = 0;
    }
  }
  va_start(ap, fmt);
  cs_vsnprintf(m->err + n, sizeof m->err - (size_t)n, fmt, ap);
  va_end(ap);
}

/* ---- types ------------------------------------------------------------- */

static const char *const prim_names[MT_PTR] = {
    "void", "u8", "u16", "u32", "u64", "i8", "i16", "i32", "i64", "uword", "f32", "f64",
};

SirType *sir_prim(SirModule *m, SirTypeKind kind) {
  if ((int)kind < 0 || kind >= MT_PTR) {
    CS_BUG("not a scalar type kind");
  }
  return &m->prim[kind];
}

SirType *sir_prim_by_name(SirModule *m, const char *name) {
  int k;
  for (k = 0; k < MT_PTR; k++) {
    if (strcmp(prim_names[k], name) == 0) {
      return &m->prim[k];
    }
  }
  return NULL;
}

SirType *sir_ptr(SirModule *m, SirType *elem) {
  if (elem->ptr_to == NULL) {
    SirType *t = (SirType *)cs_arena_new(&m->arena, sizeof(SirType));
    t->kind = MT_PTR;
    t->elem = elem;
    elem->ptr_to = t;
  }
  return elem->ptr_to;
}

SirType *sir_array(SirModule *m, SirType *elem, uint64_t count) {
  SirType *t = (SirType *)cs_arena_new(&m->arena, sizeof(SirType));
  t->kind = MT_ARRAY;
  t->elem = elem;
  t->count = count;
  return t;
}

SirStruct *sir_struct_ref(SirModule *m, const char *name) {
  SirStruct *s;
  for (s = m->structs; s != NULL; s = s->next) {
    if (strcmp(s->name, name) == 0) {
      return s;
    }
  }
  s = (SirStruct *)cs_arena_new(&m->arena, sizeof(SirStruct));
  s->name = cs_arena_str(&m->arena, name);
  s->type.kind = MT_STRUCT;
  s->type.st = s;
  if (m->structs_tail != NULL) {
    m->structs_tail->next = s;
  } else {
    m->structs = s;
  }
  m->structs_tail = s;
  return s;
}

SirStruct *sir_struct_define(SirModule *m, const char *name, SirField *fields, size_t nfields, int line) {
  SirStruct *s = sir_struct_ref(m, name);
  if (s->defined) {
    sir_error(m, line, "E1144", "struct %s is defined twice", name);
    return s;
  }
  s->defined = true;
  s->fields = fields;
  s->nfields = nfields;
  s->line = line;
  return s;
}

bool sir_type_eq(const SirType *a, const SirType *b) {
  if (a == b) {
    return true;
  }
  if (a->kind != b->kind) {
    return false;
  }
  switch (a->kind) {
  case MT_PTR:
    return sir_type_eq(a->elem, b->elem);
  case MT_ARRAY:
    return a->count == b->count && sir_type_eq(a->elem, b->elem);
  case MT_STRUCT:
    return a->st == b->st;
  default:
    return true;
  }
}

bool sir_type_is_int(const SirType *t) {
  return t->kind >= MT_U8 && t->kind <= MT_UWORD;
}

bool sir_type_is_signed(const SirType *t) {
  return t->kind >= MT_I8 && t->kind <= MT_I64;
}

bool sir_type_is_scalar(const SirType *t) {
  return sir_type_is_int(t) || t->kind == MT_PTR;
}

void sir_type_print(CsBuf *b, const SirType *t) {
  switch (t->kind) {
  case MT_PTR:
    cs_buf_puts(b, "ptr<");
    sir_type_print(b, t->elem);
    cs_buf_putc(b, '>');
    break;
  case MT_ARRAY:
    cs_buf_printf(b, "[%" PRIu64 " x ", t->count);
    sir_type_print(b, t->elem);
    cs_buf_putc(b, ']');
    break;
  case MT_STRUCT:
    cs_buf_puts(b, t->st->name);
    break;
  default:
    cs_buf_puts(b, prim_names[t->kind]);
    break;
  }
}

static const char *const op_names[ME_OP_COUNT] = {
    "const", "local", "global", "addr", "load", "index", "field", "call", "add",  "sub",    "mul",
    "div",   "rem",   "and",    "or",   "xor",  "shl",   "shr",   "not",  "neg",  "eq",     "ne",
    "lt",    "le",    "gt",     "ge",   "land", "lor",   "lnot",  "as",   "sizeof", "alignof",
};

const char *sir_op_name(SirOp op) {
  if ((int)op < 0 || op >= ME_OP_COUNT) {
    CS_BUG("not an operation");
  }
  return op_names[op];
}

/* ---- declarations ------------------------------------------------------ */

SirGlobal *sir_global_add(SirModule *m, const char *name, SirType *type, bool is_const, int line) {
  SirGlobal *g = (SirGlobal *)cs_arena_new(&m->arena, sizeof(SirGlobal));
  g->name = cs_arena_str(&m->arena, name);
  g->type = type;
  g->is_const = is_const;
  g->line = line;
  if (m->globals_tail != NULL) {
    m->globals_tail->next = g;
  } else {
    m->globals = g;
  }
  m->globals_tail = g;
  return g;
}

SirFunc *sir_func_add(SirModule *m, const char *name, SirParam *params, size_t nparams, SirType *ret, int line) {
  SirFunc *f = (SirFunc *)cs_arena_new(&m->arena, sizeof(SirFunc));
  f->name = cs_arena_str(&m->arena, name);
  f->params = params;
  f->nparams = nparams;
  f->ret = ret;
  f->line = line;
  if (m->funcs_tail != NULL) {
    m->funcs_tail->next = f;
  } else {
    m->funcs = f;
  }
  m->funcs_tail = f;
  return f;
}

/* ---- expressions and statements ---------------------------------------- */

SirExpr *sir_expr(SirModule *m, SirOp op, int line) {
  SirExpr *e = (SirExpr *)cs_arena_new(&m->arena, sizeof(SirExpr));
  e->op = op;
  e->line = line;
  return e;
}

SirExpr *sir_const(SirModule *m, SirType *type, uint64_t value, int line) {
  SirExpr *e = sir_expr(m, ME_CONST, line);
  e->type = type;
  e->ival = value;
  return e;
}

SirExpr *sir_local_ref(SirModule *m, const char *name, int line) {
  SirExpr *e = sir_expr(m, ME_LOCAL, line);
  e->name = cs_arena_str(&m->arena, name);
  return e;
}

SirExpr *sir_global_ref(SirModule *m, const char *name, int line) {
  SirExpr *e = sir_expr(m, ME_GLOBAL, line);
  e->name = cs_arena_str(&m->arena, name);
  return e;
}

SirExpr *sir_unary(SirModule *m, SirOp op, SirExpr *a, int line) {
  SirExpr *e = sir_expr(m, op, line);
  e->a = a;
  return e;
}

SirExpr *sir_binary(SirModule *m, SirOp op, SirExpr *a, SirExpr *b, int line) {
  SirExpr *e = sir_expr(m, op, line);
  e->a = a;
  e->b = b;
  return e;
}

SirExpr *sir_field(SirModule *m, SirExpr *ptr, const char *name, int line) {
  SirExpr *e = sir_expr(m, ME_FIELD, line);
  e->a = ptr;
  e->name = cs_arena_str(&m->arena, name);
  return e;
}

SirExpr *sir_call(SirModule *m, const char *callee, SirExpr **args, size_t nargs, int line) {
  SirExpr *e = sir_expr(m, ME_CALL, line);
  e->name = cs_arena_str(&m->arena, callee);
  e->args = args;
  e->nargs = nargs;
  return e;
}

SirExpr *sir_as(SirModule *m, SirType *to, SirExpr *a, int line) {
  SirExpr *e = sir_expr(m, ME_AS, line);
  e->targ = to;
  e->a = a;
  return e;
}

SirExpr *sir_type_op(SirModule *m, SirOp op, SirType *t, int line) {
  SirExpr *e = sir_expr(m, op, line);
  e->targ = t;
  return e;
}

SirStmt *sir_stmt(SirModule *m, SirStmtKind kind, int line) {
  SirStmt *s = (SirStmt *)cs_arena_new(&m->arena, sizeof(SirStmt));
  s->kind = kind;
  s->line = line;
  return s;
}

void sir_stmt_loc(SirModule *m, SirStmt *s, const char *file, uint32_t line, uint32_t col) {
  if (line == 0 || line >= SIR_LOC_MAX || col == 0 || col >= SIR_LOC_MAX || file[0] == '\0') {
    return;
  }
  /* consecutive statements share one copy of the name */
  if (m->loc_file == NULL || strcmp(m->loc_file, file) != 0) {
    m->loc_file = cs_arena_str(&m->arena, file);
  }
  s->loc_file = m->loc_file;
  s->loc_line = line;
  s->loc_col = col;
}

/* ---- targets and layout ------------------------------------------------ */

/* The targets of the pinned QBE. All are LP64-style: 8-byte pointers, and
 * every scalar aligned to its size. */
static const SirTarget targets[] = {
    {"amd64_sysv", 8, SIR_ELF}, {"amd64_apple", 8, SIR_MACHO}, {"amd64_win", 8, SIR_COFF},
    {"arm64", 8, SIR_ELF},      {"arm64_apple", 8, SIR_MACHO}, {"rv64", 8, SIR_ELF},
};

const SirTarget *sir_target_by_name(const char *name) {
  size_t i;
  for (i = 0; i < sizeof targets / sizeof targets[0]; i++) {
    if (strcmp(targets[i].name, name) == 0) {
      return &targets[i];
    }
  }
  return NULL;
}

const SirTarget *sir_target_host(void) {
  /* Predefined macros are the only portable way to ask; an unknown host
   * gets no default and must name a target. */
#if defined(_WIN32) && (defined(__x86_64__) || defined(_M_X64))
  return sir_target_by_name("amd64_win");
#elif defined(__APPLE__) && defined(__x86_64__)
  return sir_target_by_name("amd64_apple");
#elif defined(__APPLE__) && defined(__aarch64__)
  return sir_target_by_name("arm64_apple");
#elif defined(__x86_64__)
  return sir_target_by_name("amd64_sysv");
#elif defined(__aarch64__) && !defined(_WIN32)
  return sir_target_by_name("arm64");
#elif defined(__riscv) && __riscv_xlen == 64
  return sir_target_by_name("rv64");
#else
  return NULL;
#endif
}

static uint64_t align_to(uint64_t n, uint64_t a) {
  return (n + a - 1) / a * a;
}

static void layout_struct(const SirTarget *t, SirStruct *s) {
  size_t i;
  uint64_t off = 0;
  uint64_t align = 1;
  if (s->layout_state == 2) {
    return;
  }
  if (s->layout_state == 1) {
    /* the verifier rejects a struct that contains itself by value */
    CS_BUG("recursive struct layout");
  }
  s->layout_state = 1;
  for (i = 0; i < s->nfields; i++) {
    uint64_t fa = sir_alignof(t, s->fields[i].type);
    off = align_to(off, fa);
    s->fields[i].offset = off;
    off += sir_sizeof(t, s->fields[i].type);
    if (fa > align) {
      align = fa;
    }
  }
  s->size = align_to(off, align);
  s->align = align;
  s->layout_state = 2;
}

uint64_t sir_sizeof(const SirTarget *t, SirType *type) {
  switch (type->kind) {
  case MT_VOID:
    return 0;
  case MT_U8:
  case MT_I8:
    return 1;
  case MT_U16:
  case MT_I16:
    return 2;
  case MT_U32:
  case MT_I32:
  case MT_F32:
    return 4;
  case MT_U64:
  case MT_I64:
  case MT_F64:
    return 8;
  case MT_UWORD:
  case MT_PTR:
    return t->ptr_size;
  case MT_ARRAY:
    return type->count * sir_sizeof(t, type->elem);
  case MT_STRUCT:
    layout_struct(t, type->st);
    return type->st->size;
  }
  CS_BUG("unknown type kind");
  return 0;
}

uint64_t sir_alignof(const SirTarget *t, SirType *type) {
  switch (type->kind) {
  case MT_VOID:
    return 1;
  case MT_ARRAY:
    return sir_alignof(t, type->elem);
  case MT_STRUCT:
    layout_struct(t, type->st);
    return type->st->align;
  default:
    return sir_sizeof(t, type);
  }
}
