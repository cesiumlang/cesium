/* Core MIR: module construction, types, targets and layout. */
#include "mir.h"

/* ---- module ------------------------------------------------------------ */

MirModule *mir_module_new(void) {
  CsArena arena = {NULL};
  MirModule *m = (MirModule *)cs_arena_new(&arena, sizeof(MirModule));
  int k;
  m->arena = arena;
  for (k = 0; k < MT_PTR; k++) {
    m->prim[k].kind = (MirTypeKind)k;
  }
  return m;
}

void mir_module_free(MirModule *m) {
  if (m != NULL) {
    /* the module itself lives in its arena */
    CsArena arena = m->arena;
    cs_arena_free(&arena);
  }
}

void mir_error(MirModule *m, int line, const char *fmt, ...) {
  va_list ap;
  int n = 0;
  if (m->failed) {
    return;
  }
  m->failed = true;
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

MirType *mir_prim(MirModule *m, MirTypeKind kind) {
  if ((int)kind < 0 || kind >= MT_PTR) {
    CS_BUG("not a scalar type kind");
  }
  return &m->prim[kind];
}

MirType *mir_prim_by_name(MirModule *m, const char *name) {
  int k;
  for (k = 0; k < MT_PTR; k++) {
    if (strcmp(prim_names[k], name) == 0) {
      return &m->prim[k];
    }
  }
  return NULL;
}

MirType *mir_ptr(MirModule *m, MirType *elem) {
  if (elem->ptr_to == NULL) {
    MirType *t = (MirType *)cs_arena_new(&m->arena, sizeof(MirType));
    t->kind = MT_PTR;
    t->elem = elem;
    elem->ptr_to = t;
  }
  return elem->ptr_to;
}

MirType *mir_array(MirModule *m, MirType *elem, uint64_t count) {
  MirType *t = (MirType *)cs_arena_new(&m->arena, sizeof(MirType));
  t->kind = MT_ARRAY;
  t->elem = elem;
  t->count = count;
  return t;
}

MirStruct *mir_struct_ref(MirModule *m, const char *name) {
  MirStruct *s;
  for (s = m->structs; s != NULL; s = s->next) {
    if (strcmp(s->name, name) == 0) {
      return s;
    }
  }
  s = (MirStruct *)cs_arena_new(&m->arena, sizeof(MirStruct));
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

MirStruct *mir_struct_define(MirModule *m, const char *name, MirField *fields, size_t nfields, int line) {
  MirStruct *s = mir_struct_ref(m, name);
  if (s->defined) {
    mir_error(m, line, "struct %s is defined twice", name);
    return s;
  }
  s->defined = true;
  s->fields = fields;
  s->nfields = nfields;
  s->line = line;
  return s;
}

bool mir_type_eq(const MirType *a, const MirType *b) {
  if (a == b) {
    return true;
  }
  if (a->kind != b->kind) {
    return false;
  }
  switch (a->kind) {
  case MT_PTR:
    return mir_type_eq(a->elem, b->elem);
  case MT_ARRAY:
    return a->count == b->count && mir_type_eq(a->elem, b->elem);
  case MT_STRUCT:
    return a->st == b->st;
  default:
    return true;
  }
}

bool mir_type_is_int(const MirType *t) {
  return t->kind >= MT_U8 && t->kind <= MT_UWORD;
}

bool mir_type_is_signed(const MirType *t) {
  return t->kind >= MT_I8 && t->kind <= MT_I64;
}

bool mir_type_is_scalar(const MirType *t) {
  return mir_type_is_int(t) || t->kind == MT_PTR;
}

void mir_type_print(CsBuf *b, const MirType *t) {
  switch (t->kind) {
  case MT_PTR:
    cs_buf_puts(b, "ptr<");
    mir_type_print(b, t->elem);
    cs_buf_putc(b, '>');
    break;
  case MT_ARRAY:
    cs_buf_printf(b, "[%" PRIu64 " x ", t->count);
    mir_type_print(b, t->elem);
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

const char *mir_op_name(MirOp op) {
  if ((int)op < 0 || op >= ME_OP_COUNT) {
    CS_BUG("not an operation");
  }
  return op_names[op];
}

/* ---- declarations ------------------------------------------------------ */

MirGlobal *mir_global_add(MirModule *m, const char *name, MirType *type, bool is_const, int line) {
  MirGlobal *g = (MirGlobal *)cs_arena_new(&m->arena, sizeof(MirGlobal));
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

MirFunc *mir_func_add(MirModule *m, const char *name, MirParam *params, size_t nparams, MirType *ret, int line) {
  MirFunc *f = (MirFunc *)cs_arena_new(&m->arena, sizeof(MirFunc));
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

MirExpr *mir_expr(MirModule *m, MirOp op, int line) {
  MirExpr *e = (MirExpr *)cs_arena_new(&m->arena, sizeof(MirExpr));
  e->op = op;
  e->line = line;
  return e;
}

MirExpr *mir_const(MirModule *m, MirType *type, uint64_t value, int line) {
  MirExpr *e = mir_expr(m, ME_CONST, line);
  e->type = type;
  e->ival = value;
  return e;
}

MirExpr *mir_local_ref(MirModule *m, const char *name, int line) {
  MirExpr *e = mir_expr(m, ME_LOCAL, line);
  e->name = cs_arena_str(&m->arena, name);
  return e;
}

MirExpr *mir_global_ref(MirModule *m, const char *name, int line) {
  MirExpr *e = mir_expr(m, ME_GLOBAL, line);
  e->name = cs_arena_str(&m->arena, name);
  return e;
}

MirExpr *mir_unary(MirModule *m, MirOp op, MirExpr *a, int line) {
  MirExpr *e = mir_expr(m, op, line);
  e->a = a;
  return e;
}

MirExpr *mir_binary(MirModule *m, MirOp op, MirExpr *a, MirExpr *b, int line) {
  MirExpr *e = mir_expr(m, op, line);
  e->a = a;
  e->b = b;
  return e;
}

MirExpr *mir_field(MirModule *m, MirExpr *ptr, const char *name, int line) {
  MirExpr *e = mir_expr(m, ME_FIELD, line);
  e->a = ptr;
  e->name = cs_arena_str(&m->arena, name);
  return e;
}

MirExpr *mir_call(MirModule *m, const char *callee, MirExpr **args, size_t nargs, int line) {
  MirExpr *e = mir_expr(m, ME_CALL, line);
  e->name = cs_arena_str(&m->arena, callee);
  e->args = args;
  e->nargs = nargs;
  return e;
}

MirExpr *mir_as(MirModule *m, MirType *to, MirExpr *a, int line) {
  MirExpr *e = mir_expr(m, ME_AS, line);
  e->targ = to;
  e->a = a;
  return e;
}

MirExpr *mir_type_op(MirModule *m, MirOp op, MirType *t, int line) {
  MirExpr *e = mir_expr(m, op, line);
  e->targ = t;
  return e;
}

MirStmt *mir_stmt(MirModule *m, MirStmtKind kind, int line) {
  MirStmt *s = (MirStmt *)cs_arena_new(&m->arena, sizeof(MirStmt));
  s->kind = kind;
  s->line = line;
  return s;
}

/* ---- targets and layout ------------------------------------------------ */

/* The targets of the pinned QBE. All are LP64-style: 8-byte pointers, and
 * every scalar aligned to its size. */
static const MirTarget targets[] = {
    {"amd64_sysv", 8}, {"amd64_apple", 8}, {"amd64_win", 8}, {"arm64", 8}, {"arm64_apple", 8}, {"rv64", 8},
};

const MirTarget *mir_target_by_name(const char *name) {
  size_t i;
  for (i = 0; i < sizeof targets / sizeof targets[0]; i++) {
    if (strcmp(targets[i].name, name) == 0) {
      return &targets[i];
    }
  }
  return NULL;
}

const MirTarget *mir_target_host(void) {
  /* Predefined macros are the only portable way to ask; an unknown host
   * gets no default and must name a target. */
#if defined(_WIN32) && (defined(__x86_64__) || defined(_M_X64))
  return mir_target_by_name("amd64_win");
#elif defined(__APPLE__) && defined(__x86_64__)
  return mir_target_by_name("amd64_apple");
#elif defined(__APPLE__) && defined(__aarch64__)
  return mir_target_by_name("arm64_apple");
#elif defined(__x86_64__)
  return mir_target_by_name("amd64_sysv");
#elif defined(__aarch64__) && !defined(_WIN32)
  return mir_target_by_name("arm64");
#elif defined(__riscv) && __riscv_xlen == 64
  return mir_target_by_name("rv64");
#else
  return NULL;
#endif
}

static uint64_t align_to(uint64_t n, uint64_t a) {
  return (n + a - 1) / a * a;
}

static void layout_struct(const MirTarget *t, MirStruct *s) {
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
    uint64_t fa = mir_alignof(t, s->fields[i].type);
    off = align_to(off, fa);
    s->fields[i].offset = off;
    off += mir_sizeof(t, s->fields[i].type);
    if (fa > align) {
      align = fa;
    }
  }
  s->size = align_to(off, align);
  s->align = align;
  s->layout_state = 2;
}

uint64_t mir_sizeof(const MirTarget *t, MirType *type) {
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
    return type->count * mir_sizeof(t, type->elem);
  case MT_STRUCT:
    layout_struct(t, type->st);
    return type->st->size;
  }
  CS_BUG("unknown type kind");
  return 0;
}

uint64_t mir_alignof(const MirTarget *t, MirType *type) {
  switch (type->kind) {
  case MT_VOID:
    return 1;
  case MT_ARRAY:
    return mir_alignof(t, type->elem);
  case MT_STRUCT:
    layout_struct(t, type->st);
    return type->st->align;
  default:
    return mir_sizeof(t, type);
  }
}
