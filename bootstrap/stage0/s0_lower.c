/* S0 checking and lowering to core MIR, following the "S0 -> core MIR
 * mapping" of cesiumlang/cesium#9 and its amendment of 2026-11-16.
 *
 * Pointers (cesiumlang/cesium#11; stage 0 parses the ownership forms and
 * enforces none of them, and treats `~T` exactly as `#T`):
 *   - a pointer is dereferenced by default. Where a pointer is expected (an
 *     argument for a pointer parameter, the right side of `:=`, `#=`, `~=`,
 *     a returned pointer, the operand of a cast to a pointer type) a pointer
 *     is used as it is; everywhere else it is read through, to a number
 *     (rule 29);
 *   - `p[i]` and `p.f` reach through a pointer;
 *   - plain `=` assigns through a pointer and never initializes one (rule 5);
 *     `:=`, `#=` and `~=` set where a pointer points (rule 2);
 *   - `#x` and `~x` take the address of a plain value (rule 20).
 *
 * Integers: there are no implicit conversions. An integer literal takes the
 * type its context gives it: the declared type, the parameter type, or the
 * type of the other operand. A literal with no such context is rejected,
 * because its default type is an open question (cesiumlang/cesium#9, open
 * question 2).
 */
#include "s0.h"

/* ---- symbols ----------------------------------------------------------- */

typedef struct Const {
  const char *name;
  MirType *type;
  uint64_t value;
  struct Const *next;
} Const;

typedef struct Global {
  const char *name;
  MirType *type;
  struct Global *next;
} Global;

typedef struct Func {
  const char *name;
  S0Decl *decl;
  MirFunc *mir;
  struct Func *next;
} Func;

typedef struct StructName {
  const char *name;
  S0Decl *decl;
  struct StructName *next;
} StructName;

typedef struct Local {
  const char *name;
  MirType *type;
  struct Local *prev;
} Local;

typedef struct L {
  CsArena *arena;
  S0Diag *diag;
  MirModule *m;
  StructName *structs;
  Const *consts;
  Global *globals;
  Func *funcs;
  /* the function being lowered */
  MirFunc *fn;
  Local *locals;
  int loops;
  int nstrings;
  bool uses_alloc;
} L;

typedef struct Val {
  bool ok;
  MirExpr *e;
  MirType *t;
} Val;

/* Somewhere a value is stored: its address, and the type stored there. */
typedef struct Place {
  bool ok;
  MirExpr *addr;
  MirType *t;
  const char *local; /* set when the place is a whole local variable */
} Place;

static Val bad_val(void) {
  Val v;
  v.ok = false;
  v.e = NULL;
  v.t = NULL;
  return v;
}

static Val make_val(MirExpr *e, MirType *t) {
  Val v;
  v.ok = true;
  v.e = e;
  v.t = t;
  return v;
}

static Place bad_place(void) {
  Place p;
  p.ok = false;
  p.addr = NULL;
  p.t = NULL;
  p.local = NULL;
  return p;
}

/* ---- types ------------------------------------------------------------- */

static int depth_of(const MirType *t) {
  int d = 0;
  while (t->kind == MT_PTR) {
    d++;
    t = t->elem;
  }
  return d;
}

/* A type as S0 writes it, for messages. */
static void type_put(CsBuf *b, const MirType *t) {
  switch (t->kind) {
  case MT_PTR:
    cs_buf_putc(b, '#');
    type_put(b, t->elem);
    break;
  case MT_ARRAY:
    type_put(b, t->elem);
    cs_buf_printf(b, "[%" PRIu64 "]", t->count);
    break;
  default:
    mir_type_print(b, t);
    break;
  }
}

static const char *type_name(L *l, const MirType *t) {
  CsBuf b = {NULL, 0, 0};
  const char *s;
  type_put(&b, t);
  s = cs_arena_str(l->arena, b.data);
  cs_buf_free(&b);
  return s;
}

static StructName *find_struct(L *l, const char *name) {
  StructName *s;
  for (s = l->structs; s != NULL; s = s->next) {
    if (strcmp(s->name, name) == 0) {
      return s;
    }
  }
  return NULL;
}

/* The MIR type of a written type, or NULL after an error. */
static MirType *resolve_type(L *l, const S0TypeSyn *syn) {
  MirType *t;
  int i;
  switch (syn->base) {
  case BT_VOID:
    t = mir_prim(l->m, MT_VOID);
    break;
  case BT_U8:
    t = mir_prim(l->m, MT_U8);
    break;
  case BT_I32:
    t = mir_prim(l->m, MT_I32);
    break;
  case BT_U32:
    t = mir_prim(l->m, MT_U32);
    break;
  case BT_I64:
    t = mir_prim(l->m, MT_I64);
    break;
  case BT_U64:
    t = mir_prim(l->m, MT_U64);
    break;
  case BT_UWORD:
    t = mir_prim(l->m, MT_UWORD);
    break;
  default:
    if (find_struct(l, syn->name) == NULL) {
      s0_error(l->diag, syn->loc, "unknown type '%s'", syn->name);
      return NULL;
    }
    t = &mir_struct_ref(l->m, syn->name)->type;
    break;
  }
  for (i = 0; i < syn->depth; i++) {
    t = mir_ptr(l->m, t);
  }
  return t;
}

static bool literal_fits(const MirType *t, uint64_t v) {
  switch (t->kind) {
  case MT_U8:
    return v <= UINT8_MAX;
  case MT_U16:
    return v <= UINT16_MAX;
  case MT_U32:
    return v <= UINT32_MAX;
  case MT_U64:
  case MT_UWORD:
    return true;
  case MT_I8:
    return v <= (uint64_t)INT8_MAX;
  case MT_I16:
    return v <= (uint64_t)INT16_MAX;
  case MT_I32:
    return v <= (uint64_t)INT32_MAX;
  case MT_I64:
    return v <= (uint64_t)INT64_MAX;
  default:
    return false;
  }
}

/* ---- lookup ------------------------------------------------------------ */

static Local *find_local(L *l, const char *name) {
  Local *v;
  for (v = l->locals; v != NULL; v = v->prev) {
    if (strcmp(v->name, name) == 0) {
      return v;
    }
  }
  return NULL;
}

static Const *find_const(L *l, const char *name) {
  Const *c;
  for (c = l->consts; c != NULL; c = c->next) {
    if (strcmp(c->name, name) == 0) {
      return c;
    }
  }
  return NULL;
}

static Global *find_global(L *l, const char *name) {
  Global *g;
  for (g = l->globals; g != NULL; g = g->next) {
    if (strcmp(g->name, name) == 0) {
      return g;
    }
  }
  return NULL;
}

static Func *find_func(L *l, const char *name) {
  Func *f;
  for (f = l->funcs; f != NULL; f = f->next) {
    if (strcmp(f->name, name) == 0) {
      return f;
    }
  }
  return NULL;
}

/* True, after reporting, when a new name collides with one in use. Whether
 * Cesium allows shadowing is not recorded anywhere, so S0 allows none. */
static bool name_taken(L *l, const char *name, S0Loc loc, bool check_locals) {
  const char *what = NULL;
  if (strcmp(name, "alloc") == 0) {
    what = "the built-in alloc";
  } else if (find_const(l, name) != NULL) {
    what = "a constant";
  } else if (find_global(l, name) != NULL) {
    what = "a global";
  } else if (find_func(l, name) != NULL) {
    what = "a function";
  } else if (find_struct(l, name) != NULL) {
    what = "a struct";
  } else if (check_locals && find_local(l, name) != NULL) {
    what = "a local or parameter";
  }
  if (what != NULL) {
    s0_error(l->diag, loc, "'%s' is already the name of %s", name, what);
    return true;
  }
  return false;
}

/* ---- expressions ------------------------------------------------------- */

/* How an expression is used. */
typedef struct Want {
  MirType *type;    /* the exact type wanted, or NULL */
  int ptr_depth;    /* type == NULL: > 0 wants any pointer of this depth (a cast's operand) */
  bool or_uword;    /* with ptr_depth: a uword is accepted too (integer -> pointer cast) */
  bool allow_void;  /* a call used as a statement */
} Want;

static Want want_value(void) {
  Want w;
  memset(&w, 0, sizeof w);
  return w;
}

static Want want_type(MirType *t) {
  Want w;
  memset(&w, 0, sizeof w);
  w.type = t;
  return w;
}

static Val lower_expr(L *l, S0Expr *e, Want w);

static bool is_untyped(const S0Expr *e) {
  if (e->kind == EX_INT) {
    return true;
  }
  if (e->kind == EX_BINARY) {
    switch (e->op) {
    case TK_PLUS:
    case TK_MINUS:
    case TK_STAR:
    case TK_SLASH:
    case TK_PERCENT:
    case TK_AMP:
    case TK_PIPE:
    case TK_XOR:
    case TK_SHL:
    case TK_SHR:
      return is_untyped(e->a) && is_untyped(e->b);
    default:
      return false;
    }
  }
  return false;
}

/* Reads through pointers until the value has the depth the context wants,
 * then checks the result against the context. */
static Val settle(L *l, S0Loc loc, MirExpr *v, MirType *t, Want w) {
  int target = w.type != NULL ? depth_of(w.type) : w.ptr_depth;
  while (depth_of(t) > target) {
    t = t->elem;
    if (!mir_type_is_scalar(t)) {
      s0_error(l->diag, loc, "a %s is not a value; use one of its %s", type_name(l, t),
               t->kind == MT_ARRAY ? "elements" : "fields");
      return bad_val();
    }
    v = mir_unary(l->m, ME_LOAD, v, loc.line);
  }
  if (w.type != NULL) {
    if (!mir_type_eq(t, w.type)) {
      s0_error(l->diag, loc, "expected %s, found %s%s", type_name(l, w.type), type_name(l, t),
               mir_type_is_int(t) && mir_type_is_int(w.type) ? "; convert it with 'as'" : "");
      return bad_val();
    }
  } else if (w.ptr_depth > 0) {
    if (!(t->kind == MT_PTR || (w.or_uword && t->kind == MT_UWORD))) {
      s0_error(l->diag, loc, "expected a pointer%s, found %s", w.or_uword ? " or a uword" : "", type_name(l, t));
      return bad_val();
    }
  } else if (!mir_type_is_int(t)) {
    s0_error(l->diag, loc, "expected a number, found %s", type_name(l, t));
    return bad_val();
  }
  return make_val(v, t);
}

static Place lower_place(L *l, S0Expr *e);

/* The value stored at a place: the local itself, or a load. */
static MirExpr *place_value(L *l, Place p, int line) {
  if (p.local != NULL) {
    return mir_local_ref(l->m, p.local, line);
  }
  return mir_unary(l->m, ME_LOAD, p.addr, line);
}

static Place lower_place(L *l, S0Expr *e) {
  Place p = bad_place();
  switch (e->kind) {
  case EX_NAME: {
    Local *v = find_local(l, e->name);
    Global *g;
    if (v != NULL) {
      p.ok = true;
      p.addr = mir_expr(l->m, ME_ADDR, e->loc.line);
      p.addr->name = v->name;
      p.t = v->type;
      p.local = v->name;
      return p;
    }
    g = find_global(l, e->name);
    if (g != NULL) {
      p.ok = true;
      p.addr = mir_global_ref(l->m, g->name, e->loc.line);
      p.t = g->type;
      return p;
    }
    if (find_const(l, e->name) != NULL) {
      s0_error(l->diag, e->loc, "'%s' is a constant: it has no address and cannot be assigned", e->name);
    } else if (find_func(l, e->name) != NULL) {
      s0_error(l->diag, e->loc, "'%s' is a function", e->name);
    } else {
      s0_error(l->diag, e->loc, "unknown name '%s'", e->name);
    }
    return p;
  }

  case EX_INDEX: {
    Place base = lower_place(l, e->a);
    Val idx;
    MirType *elem;
    MirExpr *first;
    if (!base.ok) {
      return p;
    }
    if (base.t->kind == MT_PTR) {
      /* p[i]: element i of what p points at */
      elem = base.t->elem;
      first = place_value(l, base, e->loc.line);
    } else if (base.t->kind == MT_ARRAY) {
      /* an array is indexed in place */
      elem = base.t->elem;
      first = mir_as(l->m, mir_ptr(l->m, elem), base.addr, e->loc.line);
    } else {
      s0_error(l->diag, e->loc, "a %s cannot be indexed", type_name(l, base.t));
      return p;
    }
    if (elem->kind == MT_VOID) {
      s0_error(l->diag, e->loc, "a pointer to void cannot be indexed");
      return p;
    }
    if (is_untyped(e->b)) {
      idx = lower_expr(l, e->b, want_type(mir_prim(l->m, MT_UWORD)));
    } else {
      idx = lower_expr(l, e->b, want_value());
    }
    if (!idx.ok) {
      return p;
    }
    p.ok = true;
    p.addr = mir_binary(l->m, ME_INDEX, first, idx.e, e->loc.line);
    p.t = elem;
    return p;
  }

  case EX_FIELD: {
    Place base = lower_place(l, e->a);
    size_t i;
    MirStruct *st;
    if (!base.ok) {
      return p;
    }
    /* a.b reaches through pointers to the struct */
    while (base.t->kind == MT_PTR) {
      base.addr = place_value(l, base, e->loc.line);
      base.t = base.t->elem;
      base.local = NULL;
    }
    if (base.t->kind != MT_STRUCT) {
      s0_error(l->diag, e->loc, "a %s has no fields", type_name(l, base.t));
      return p;
    }
    st = base.t->st;
    for (i = 0; i < st->nfields; i++) {
      if (strcmp(st->fields[i].name, e->name) == 0) {
        p.ok = true;
        p.addr = mir_field(l->m, base.addr, e->name, e->loc.line);
        p.t = st->fields[i].type;
        return p;
      }
    }
    s0_error(l->diag, e->loc, "struct %s has no field '%s'", st->name, e->name);
    return p;
  }

  default:
    s0_error(l->diag, e->loc, "this expression is not a variable, an element or a field");
    return p;
  }
}

static bool is_place_expr(const S0Expr *e) {
  return e->kind == EX_NAME || e->kind == EX_INDEX || e->kind == EX_FIELD;
}

static MirOp binary_op(S0Tok op) {
  switch (op) {
  case TK_PLUS:
    return ME_ADD;
  case TK_MINUS:
    return ME_SUB;
  case TK_STAR:
    return ME_MUL;
  case TK_SLASH:
    return ME_DIV;
  case TK_PERCENT:
    return ME_REM;
  case TK_AMP:
    return ME_AND;
  case TK_PIPE:
    return ME_OR;
  case TK_XOR:
    return ME_XOR;
  case TK_SHL:
    return ME_SHL;
  case TK_SHR:
    return ME_SHR;
  case TK_EQ:
    return ME_EQ;
  case TK_NE:
    return ME_NE;
  case TK_LT:
    return ME_LT;
  case TK_LE:
    return ME_LE;
  case TK_GT:
    return ME_GT;
  case TK_GE:
    return ME_GE;
  case TK_ANDAND:
    return ME_LAND;
  case TK_OROR:
    return ME_LOR;
  default:
    CS_BUG("not a binary operator");
    return ME_ADD;
  }
}

static bool is_comparison(S0Tok op) {
  return op == TK_EQ || op == TK_NE || op == TK_LT || op == TK_LE || op == TK_GT || op == TK_GE;
}

/* The two operands of an arithmetic or comparison operator: both numbers of
 * one type. A literal takes the other operand's type. `fallback` is the type
 * for two literals, or NULL when the context gives none. */
static bool lower_operands(L *l, S0Expr *e, MirType *fallback, Val *a, Val *b) {
  bool lu = is_untyped(e->a);
  bool ru = is_untyped(e->b);
  if (lu && ru) {
    if (fallback == NULL || !mir_type_is_int(fallback)) {
      s0_error(l->diag, e->loc, "the type of these literals is not determined by their context; write 'as T' after one");
      return false;
    }
    *a = lower_expr(l, e->a, want_type(fallback));
    *b = lower_expr(l, e->b, want_type(fallback));
  } else if (lu) {
    /* the literal on the left takes the right operand's type; the MIR
     * still evaluates left to right */
    *b = lower_expr(l, e->b, want_value());
    if (!b->ok) {
      return false;
    }
    *a = lower_expr(l, e->a, want_type(b->t));
  } else if (ru) {
    *a = lower_expr(l, e->a, want_value());
    if (!a->ok) {
      return false;
    }
    *b = lower_expr(l, e->b, want_type(a->t));
  } else {
    *a = lower_expr(l, e->a, want_value());
    *b = lower_expr(l, e->b, want_value());
    if (a->ok && b->ok && !mir_type_eq(a->t, b->t)) {
      s0_error(l->diag, e->loc, "the operands are %s and %s; convert one with 'as'", type_name(l, a->t),
               type_name(l, b->t));
      return false;
    }
  }
  return a->ok && b->ok;
}

static Val lower_string(L *l, S0Expr *e) {
  char name[32];
  MirGlobal *g;
  MirType *u8 = mir_prim(l->m, MT_U8);
  int n = snprintf(name, sizeof name, "str.%d", l->nstrings++);
  if (n < 0 || (size_t)n >= sizeof name) {
    CS_BUG("string name");
  }
  g = mir_global_add(l->m, name, mir_array(l->m, u8, (uint64_t)e->len + 1), true, e->loc.line);
  g->init = MI_BYTES;
  /* the arena copy is NUL-terminated: len + 1 bytes are the string and its NUL */
  g->bytes = cs_arena_strn(l->arena, e->name, e->len);
  g->nbytes = e->len + 1;
  return make_val(mir_as(l->m, mir_ptr(l->m, u8), mir_global_ref(l->m, name, e->loc.line), e->loc.line),
                  mir_ptr(l->m, u8));
}

static Val lower_call(L *l, S0Expr *e, Want w) {
  Func *f = find_func(l, e->name);
  MirExpr **args;
  size_t i;
  bool ok = true;
  if (strcmp(e->name, "alloc") == 0) {
    s0_error(l->diag, e->loc, "alloc(count) can only initialize or be assigned to a pointer with ':='");
    return bad_val();
  }
  if (f == NULL) {
    s0_error(l->diag, e->loc, "unknown function '%s'", e->name);
    return bad_val();
  }
  if (e->nargs != f->mir->nparams) {
    s0_error(l->diag, e->loc, "'%s' takes %lu arguments, not %lu", e->name, (unsigned long)f->mir->nparams,
             (unsigned long)e->nargs);
    return bad_val();
  }
  args = (MirExpr **)cs_arena_new(&l->m->arena, cs_mul_size(e->nargs == 0 ? 1 : e->nargs, sizeof(MirExpr *)));
  for (i = 0; i < e->nargs; i++) {
    Val a = lower_expr(l, e->args[i], want_type(f->mir->params[i].type));
    if (!a.ok) {
      ok = false;
    } else {
      args[i] = a.e;
    }
  }
  if (!ok) {
    return bad_val();
  }
  if (f->mir->ret->kind == MT_VOID) {
    if (!w.allow_void) {
      s0_error(l->diag, e->loc, "'%s' returns nothing", e->name);
      return bad_val();
    }
    return make_val(mir_call(l->m, e->name, args, e->nargs, e->loc.line), f->mir->ret);
  }
  if (w.allow_void) {
    /* a statement: the result is dropped as it is */
    return make_val(mir_call(l->m, e->name, args, e->nargs, e->loc.line), f->mir->ret);
  }
  return settle(l, e->loc, mir_call(l->m, e->name, args, e->nargs, e->loc.line), f->mir->ret, w);
}

static Val lower_as(L *l, S0Expr *e, Want w) {
  MirType *to = resolve_type(l, &e->type);
  Val v;
  if (to == NULL) {
    return bad_val();
  }
  if (to->kind == MT_PTR) {
    Want ow = want_value();
    ow.ptr_depth = depth_of(to);
    ow.or_uword = true;
    if (is_untyped(e->a)) {
      v = lower_expr(l, e->a, want_type(mir_prim(l->m, MT_UWORD)));
    } else {
      v = lower_expr(l, e->a, ow);
    }
  } else if (mir_type_is_int(to)) {
    if (is_untyped(e->a)) {
      /* a literal simply takes the type */
      v = lower_expr(l, e->a, want_type(to));
      if (!v.ok) {
        return v;
      }
      return settle(l, e->loc, v.e, v.t, w);
    }
    if (e->a->kind == EX_ADDROF || e->a->kind == EX_STRING) {
      /* an explicit address converts to uword only */
      Want ow = want_value();
      ow.ptr_depth = 1;
      if (to->kind != MT_UWORD) {
        s0_error(l->diag, e->loc, "an address converts to uword only, not to %s", type_name(l, to));
        return bad_val();
      }
      v = lower_expr(l, e->a, ow);
    } else {
      v = lower_expr(l, e->a, want_value());
    }
  } else {
    s0_error(l->diag, e->loc, "'as' converts to a number or a pointer type, not to %s", type_name(l, to));
    return bad_val();
  }
  if (!v.ok) {
    return v;
  }
  if (!mir_type_eq(v.t, to)) {
    v.e = mir_as(l->m, to, v.e, e->loc.line);
    v.t = to;
  }
  return settle(l, e->loc, v.e, v.t, w);
}

static Val lower_expr(L *l, S0Expr *e, Want w) {
  switch (e->kind) {
  case EX_INT:
    if (w.type == NULL || !mir_type_is_int(w.type)) {
      if (w.type != NULL) {
        s0_error(l->diag, e->loc, "expected %s, found a number", type_name(l, w.type));
      } else {
        s0_error(l->diag, e->loc,
                 "the type of this literal is not determined by its context; write 'as T' after it");
      }
      return bad_val();
    }
    if (!literal_fits(w.type, e->ival)) {
      s0_error(l->diag, e->loc, "%" PRIu64 " does not fit %s", e->ival, type_name(l, w.type));
      return bad_val();
    }
    return make_val(mir_const(l->m, w.type, e->ival, e->loc.line), w.type);

  case EX_STRING: {
    Val v = lower_string(l, e);
    if (w.type == NULL && w.ptr_depth == 0) {
      s0_error(l->diag, e->loc, "a string is a #u8, not a number");
      return bad_val();
    }
    return settle(l, e->loc, v.e, v.t, w);
  }

  case EX_NAME: {
    Const *c = find_local(l, e->name) == NULL ? find_const(l, e->name) : NULL;
    Place p;
    if (c != NULL) {
      return settle(l, e->loc, mir_const(l->m, c->type, c->value, e->loc.line), c->type, w);
    }
    p = lower_place(l, e);
    if (!p.ok) {
      return bad_val();
    }
    if (!mir_type_is_scalar(p.t)) {
      s0_error(l->diag, e->loc, "'%s' is a %s, not a value; use one of its %s", e->name, type_name(l, p.t),
               p.t->kind == MT_ARRAY ? "elements" : "fields");
      return bad_val();
    }
    return settle(l, e->loc, place_value(l, p, e->loc.line), p.t, w);
  }

  case EX_INDEX:
  case EX_FIELD: {
    Place p = lower_place(l, e);
    if (!p.ok) {
      return bad_val();
    }
    if (!mir_type_is_scalar(p.t)) {
      s0_error(l->diag, e->loc, "a %s is not a value; use one of its %s", type_name(l, p.t),
               p.t->kind == MT_ARRAY ? "elements" : "fields");
      return bad_val();
    }
    return settle(l, e->loc, place_value(l, p, e->loc.line), p.t, w);
  }

  case EX_CALL:
    return lower_call(l, e, w);

  case EX_ADDROF: {
    Place p;
    MirType *t;
    if (!is_place_expr(e->a)) {
      s0_error(l->diag, e->loc, "'#' and '~' take the address of a variable, an element or a field");
      return bad_val();
    }
    p = lower_place(l, e->a);
    if (!p.ok) {
      return bad_val();
    }
    t = mir_ptr(l->m, p.t);
    /* an explicit address is never read through */
    if (w.type != NULL) {
      if (!mir_type_eq(t, w.type)) {
        s0_error(l->diag, e->loc, "expected %s, found %s", type_name(l, w.type), type_name(l, t));
        return bad_val();
      }
    } else if (w.ptr_depth == 0) {
      s0_error(l->diag, e->loc, "an address is not a number; a pointer is expected only where one is declared");
      return bad_val();
    }
    return make_val(p.addr, t);
  }

  case EX_NOT: {
    Val a = lower_expr(l, e->a, want_value());
    if (!a.ok) {
      return a;
    }
    return settle(l, e->loc, mir_unary(l->m, ME_LNOT, a.e, e->loc.line), mir_prim(l->m, MT_U8), w);
  }

  case EX_BINARY: {
    Val a;
    Val b;
    MirOp op = binary_op(e->op);
    if (e->op == TK_ANDAND || e->op == TK_OROR) {
      a = lower_expr(l, e->a, want_value());
      b = lower_expr(l, e->b, want_value());
      if (!a.ok || !b.ok) {
        return bad_val();
      }
      return settle(l, e->loc, mir_binary(l->m, op, a.e, b.e, e->loc.line), mir_prim(l->m, MT_U8), w);
    }
    if (is_comparison(e->op)) {
      if (!lower_operands(l, e, NULL, &a, &b)) {
        return bad_val();
      }
      return settle(l, e->loc, mir_binary(l->m, op, a.e, b.e, e->loc.line), mir_prim(l->m, MT_U8), w);
    }
    if (!lower_operands(l, e, w.type, &a, &b)) {
      return bad_val();
    }
    return settle(l, e->loc, mir_binary(l->m, op, a.e, b.e, e->loc.line), a.t, w);
  }

  case EX_AS:
    return lower_as(l, e, w);

  case EX_SIZEOF: {
    MirType *t = resolve_type(l, &e->type);
    if (t == NULL) {
      return bad_val();
    }
    return settle(l, e->loc, mir_type_op(l->m, ME_SIZEOF, t, e->loc.line), mir_prim(l->m, MT_UWORD), w);
  }
  }
  CS_BUG("unknown expression");
  return bad_val();
}

/* alloc(count) for a pointer of type `to` (cesiumlang/cesium#9, mapping):
 *   as(ptr<T>, call @cs_arena_alloc(mul(n, sizeof(T)), alignof(T))) */
static Val lower_alloc(L *l, S0Expr *e, MirType *to) {
  MirType *uword = mir_prim(l->m, MT_UWORD);
  MirExpr **args;
  Val n;
  if (e->nargs != 1) {
    s0_error(l->diag, e->loc, "alloc takes one argument, the element count");
    return bad_val();
  }
  if (to->elem->kind == MT_VOID) {
    s0_error(l->diag, e->loc, "alloc needs an element type; the pointer is #void");
    return bad_val();
  }
  n = lower_expr(l, e->args[0], want_type(uword));
  if (!n.ok) {
    return n;
  }
  l->uses_alloc = true;
  args = (MirExpr **)cs_arena_new(&l->m->arena, 2 * sizeof(MirExpr *));
  args[0] = mir_binary(l->m, ME_MUL, n.e, mir_type_op(l->m, ME_SIZEOF, to->elem, e->loc.line), e->loc.line);
  args[1] = mir_type_op(l->m, ME_ALIGNOF, to->elem, e->loc.line);
  return make_val(mir_as(l->m, to, mir_call(l->m, "cs_arena_alloc", args, 2, e->loc.line), e->loc.line), to);
}

static bool is_alloc(const S0Expr *e) {
  return e->kind == EX_CALL && strcmp(e->name, "alloc") == 0;
}

/* The right side of `:=`, `#=` or `~=` for a pointer of type `to`. */
static Val lower_pointer_value(L *l, S0Expr *e, S0Tok op, MirType *to) {
  if (is_alloc(e)) {
    if (op != TK_OWN) {
      s0_error(l->diag, e->loc, "alloc returns ownership; receive it with ':='");
      return bad_val();
    }
    return lower_alloc(l, e, to);
  }
  return lower_expr(l, e, want_type(to));
}

/* ---- statements -------------------------------------------------------- */

typedef struct Block {
  MirStmt *head;
  MirStmt *tail;
} Block;

static void block_add(Block *b, MirStmt *s) {
  if (b->tail != NULL) {
    b->tail->next = s;
  } else {
    b->head = s;
  }
  b->tail = s;
}

static MirStmt *lower_block(L *l, S0Stmt *s);

static void lower_decl(L *l, S0Stmt *s, Block *out) {
  MirType *t = resolve_type(l, &s->type);
  MirStmt *ms;
  Local *v;
  Val init = bad_val();
  bool has_init = s->op != TK_EOF;
  if (t == NULL) {
    return;
  }
  if (s->is_array) {
    if (s->count == 0) {
      s0_error(l->diag, s->loc, "an array has at least one element");
      return;
    }
    t = mir_array(l->m, t, s->count);
  }
  if (has_init) {
    if (!mir_type_is_scalar(t)) {
      s0_error(l->diag, s->loc, "a %s has no initializer in S0; assign its %s one at a time", type_name(l, t),
               t->kind == MT_ARRAY ? "elements" : "fields");
      return;
    }
    if (t->kind == MT_PTR) {
      if (s->op == TK_ASSIGN) {
        s0_error(l->diag, s->loc, "a pointer is initialized with ':=', '#=' or '~='; plain '=' assigns through it");
        return;
      }
      init = lower_pointer_value(l, s->b, s->op, t);
    } else {
      if (s->op != TK_ASSIGN) {
        s0_error(l->diag, s->loc, "%s is only for pointers; a plain value is initialized with '='",
                 s0_tok_name(s->op));
        return;
      }
      init = lower_expr(l, s->b, want_type(t));
    }
    if (!init.ok) {
      return;
    }
  }
  /* the name comes into scope after its initializer */
  if (name_taken(l, s->name, s->loc, true)) {
    return;
  }
  ms = mir_stmt(l->m, MS_LOCAL, s->loc.line);
  ms->name = s->name;
  ms->type = t;
  ms->a = has_init ? init.e : NULL;
  block_add(out, ms);
  v = (Local *)cs_arena_new(l->arena, sizeof(Local));
  v->name = s->name;
  v->type = t;
  v->prev = l->locals;
  l->locals = v;
}

static void lower_assign(L *l, S0Stmt *s, Block *out) {
  Place p;
  Val v;
  MirStmt *ms;
  if (!is_place_expr(s->a)) {
    s0_error(l->diag, s->loc, "the left side of an assignment is a variable, an element or a field");
    return;
  }
  p = lower_place(l, s->a);
  if (!p.ok) {
    return;
  }
  if (s->op == TK_ASSIGN) {
    /* plain '=' stores a number, through pointers if the place holds one */
    while (p.t->kind == MT_PTR) {
      p.addr = place_value(l, p, s->loc.line);
      p.t = p.t->elem;
      p.local = NULL;
    }
    if (!mir_type_is_int(p.t)) {
      s0_error(l->diag, s->loc, "a %s is not assigned as a whole in S0; assign its %s", type_name(l, p.t),
               p.t->kind == MT_ARRAY ? "elements" : "fields");
      return;
    }
    v = lower_expr(l, s->b, want_type(p.t));
  } else {
    if (p.t->kind != MT_PTR) {
      s0_error(l->diag, s->loc, "%s sets where a pointer points; the left side is a %s", s0_tok_name(s->op),
               type_name(l, p.t));
      return;
    }
    v = lower_pointer_value(l, s->b, s->op, p.t);
  }
  if (!v.ok) {
    return;
  }
  if (p.local != NULL) {
    ms = mir_stmt(l->m, MS_SET, s->loc.line);
    ms->name = p.local;
    ms->a = v.e;
  } else {
    ms = mir_stmt(l->m, MS_STORE, s->loc.line);
    ms->a = p.addr;
    ms->b = v.e;
  }
  block_add(out, ms);
}

static void lower_stmt(L *l, S0Stmt *s, Block *out) {
  MirStmt *ms;
  Val v;
  switch (s->kind) {
  case ST_DECL:
    lower_decl(l, s, out);
    return;

  case ST_ASSIGN:
    lower_assign(l, s, out);
    return;

  case ST_EXPR: {
    Want w = want_value();
    if (s->a->kind != EX_CALL) {
      s0_error(l->diag, s->loc, "only a call can be a statement");
      return;
    }
    w.allow_void = true;
    v = lower_call(l, s->a, w);
    if (!v.ok) {
      return;
    }
    ms = mir_stmt(l->m, MS_EVAL, s->loc.line);
    ms->a = v.e;
    block_add(out, ms);
    return;
  }

  case ST_IF:
    v = lower_expr(l, s->a, want_value());
    ms = mir_stmt(l->m, MS_IF, s->loc.line);
    ms->body = lower_block(l, s->body);
    /* `else if` parses as an else block holding one if statement */
    ms->els = lower_block(l, s->els);
    if (!v.ok) {
      return;
    }
    ms->a = v.e;
    block_add(out, ms);
    return;

  case ST_WHILE:
    v = lower_expr(l, s->a, want_value());
    ms = mir_stmt(l->m, MS_WHILE, s->loc.line);
    l->loops++;
    ms->body = lower_block(l, s->body);
    l->loops--;
    if (!v.ok) {
      return;
    }
    ms->a = v.e;
    block_add(out, ms);
    return;

  case ST_BREAK:
  case ST_CONTINUE:
    if (l->loops == 0) {
      s0_error(l->diag, s->loc, "'%s' outside a loop", s->kind == ST_BREAK ? "break" : "continue");
      return;
    }
    block_add(out, mir_stmt(l->m, s->kind == ST_BREAK ? MS_BREAK : MS_CONTINUE, s->loc.line));
    return;

  case ST_RETURN:
    ms = mir_stmt(l->m, MS_RETURN, s->loc.line);
    if (s->a == NULL) {
      if (l->fn->ret->kind != MT_VOID) {
        s0_error(l->diag, s->loc, "'%s' returns a %s", l->fn->name, type_name(l, l->fn->ret));
        return;
      }
    } else {
      if (l->fn->ret->kind == MT_VOID) {
        s0_error(l->diag, s->loc, "'%s' returns nothing", l->fn->name);
        return;
      }
      v = lower_expr(l, s->a, want_type(l->fn->ret));
      if (!v.ok) {
        return;
      }
      ms->a = v.e;
    }
    block_add(out, ms);
    return;
  }
  CS_BUG("unknown statement");
}

static MirStmt *lower_block(L *l, S0Stmt *s) {
  Local *saved = l->locals;
  Block b = {NULL, NULL};
  for (; s != NULL; s = s->next) {
    lower_stmt(l, s, &b);
  }
  l->locals = saved;
  return b.head;
}

/* ---- declarations ------------------------------------------------------ */

static void declare_structs(L *l, S0Decl *decls) {
  S0Decl *d;
  /* names first, so fields can point at structs declared later */
  for (d = decls; d != NULL; d = d->next) {
    if (d->kind == DK_STRUCT) {
      StructName *s;
      if (find_struct(l, d->name) != NULL) {
        s0_error(l->diag, d->loc, "struct '%s' is declared twice", d->name);
        continue;
      }
      s = (StructName *)cs_arena_new(l->arena, sizeof(StructName));
      s->name = d->name;
      s->decl = d;
      s->next = l->structs;
      l->structs = s;
    }
  }
  for (d = decls; d != NULL; d = d->next) {
    S0Stmt *f;
    MirField *fields;
    size_t n = 0;
    size_t i = 0;
    bool ok = true;
    if (d->kind != DK_STRUCT || find_struct(l, d->name)->decl != d) {
      continue;
    }
    for (f = d->fields; f != NULL; f = f->next) {
      n++;
    }
    if (n == 0) {
      s0_error(l->diag, d->loc, "struct '%s' has no fields", d->name);
      continue;
    }
    fields = (MirField *)cs_arena_new(&l->m->arena, cs_mul_size(n, sizeof(MirField)));
    for (f = d->fields; f != NULL; f = f->next) {
      MirType *t = resolve_type(l, &f->type);
      size_t j;
      if (t == NULL) {
        ok = false;
        continue;
      }
      if (f->is_array) {
        if (f->count == 0) {
          s0_error(l->diag, f->loc, "an array has at least one element");
          ok = false;
          continue;
        }
        t = mir_array(l->m, t, f->count);
      }
      for (j = 0; j < i; j++) {
        if (strcmp(fields[j].name, f->name) == 0) {
          s0_error(l->diag, f->loc, "struct '%s' has two fields named '%s'", d->name, f->name);
          ok = false;
        }
      }
      fields[i].name = f->name;
      fields[i].type = t;
      i++;
    }
    if (ok) {
      mir_struct_define(l->m, d->name, fields, n, d->loc.line);
    } else {
      /* keep later uses from cascading: define it with what resolved */
      mir_struct_define(l->m, d->name, fields, i, d->loc.line);
    }
  }
}

static void declare_const(L *l, S0Decl *d) {
  MirType *t = resolve_type(l, &d->type);
  Const *c;
  uint64_t value;
  if (t == NULL) {
    return;
  }
  if (!mir_type_is_int(t)) {
    s0_error(l->diag, d->loc, "a constant is a number in S0, not a %s", type_name(l, t));
    return;
  }
  if (d->init->kind == EX_INT) {
    value = d->init->ival;
    if (!literal_fits(t, value)) {
      s0_error(l->diag, d->init->loc, "%" PRIu64 " does not fit %s", value, type_name(l, t));
      return;
    }
  } else if (d->init->kind == EX_NAME && find_const(l, d->init->name) != NULL) {
    Const *other = find_const(l, d->init->name);
    if (!mir_type_eq(other->type, t)) {
      s0_error(l->diag, d->init->loc, "'%s' is a %s, not a %s", other->name, type_name(l, other->type),
               type_name(l, t));
      return;
    }
    value = other->value;
  } else {
    s0_error(l->diag, d->init->loc, "a constant is initialized with a literal or another constant in S0");
    return;
  }
  if (name_taken(l, d->name, d->loc, false)) {
    return;
  }
  c = (Const *)cs_arena_new(l->arena, sizeof(Const));
  c->name = d->name;
  c->type = t;
  c->value = value;
  c->next = l->consts;
  l->consts = c;
}

static void declare_global(L *l, S0Decl *d) {
  MirType *t = resolve_type(l, &d->type);
  MirGlobal *g;
  Global *sym;
  if (t == NULL) {
    return;
  }
  if (d->is_array) {
    if (d->count == 0) {
      s0_error(l->diag, d->loc, "an array has at least one element");
      return;
    }
    t = mir_array(l->m, t, d->count);
  }
  if (name_taken(l, d->name, d->loc, false)) {
    return;
  }
  g = mir_global_add(l->m, d->name, t, false, d->loc.line);
  sym = (Global *)cs_arena_new(l->arena, sizeof(Global));
  sym->name = d->name;
  sym->type = t;
  sym->next = l->globals;
  l->globals = sym;
  if (d->op == TK_EOF) {
    return;
  }
  if (mir_type_is_int(t)) {
    Const *c = d->init->kind == EX_NAME ? find_const(l, d->init->name) : NULL;
    if (d->op != TK_ASSIGN) {
      s0_error(l->diag, d->loc, "%s is only for pointers; a plain value is initialized with '='", s0_tok_name(d->op));
    } else if (d->init->kind == EX_INT && literal_fits(t, d->init->ival)) {
      g->init = MI_INT;
      g->ival = d->init->ival;
    } else if (c != NULL && mir_type_eq(c->type, t)) {
      g->init = MI_INT;
      g->ival = c->value;
    } else {
      s0_error(l->diag, d->init->loc, "a global is initialized with a literal or a constant of its type in S0");
    }
  } else if (t->kind == MT_PTR) {
    if (d->op == TK_ASSIGN) {
      s0_error(l->diag, d->loc, "a pointer is initialized with ':=', '#=' or '~='; plain '=' assigns through it");
    } else if (d->init->kind == EX_STRING && t->elem->kind == MT_U8) {
      Val v = lower_string(l, d->init);
      /* lower_string made the bytes global and an `as(ptr<u8>, @str.N)` expression */
      g->init = MI_ADDR;
      g->addr_name = v.e->a->name;
    } else {
      s0_error(l->diag, d->init->loc, "a global pointer is initialized with a string in S0, or not at all");
    }
  } else {
    s0_error(l->diag, d->loc, "a %s has no initializer in S0", type_name(l, t));
  }
}

static void declare_func(L *l, S0Decl *d) {
  MirType *ret = resolve_type(l, &d->type);
  MirParam *params = (MirParam *)cs_arena_new(&l->m->arena, cs_mul_size(d->nparams == 0 ? 1 : d->nparams, sizeof(MirParam)));
  Func *f;
  size_t i;
  size_t j;
  bool ok = ret != NULL;
  if (ok && ret->kind != MT_VOID && !mir_type_is_scalar(ret)) {
    s0_error(l->diag, d->loc, "'%s' returns a %s; S0 functions return a number, a pointer or nothing", d->name,
             type_name(l, ret));
    ok = false;
  }
  for (i = 0; i < d->nparams; i++) {
    MirType *t = resolve_type(l, &d->params[i].type);
    if (t == NULL) {
      ok = false;
      continue;
    }
    if (!mir_type_is_scalar(t)) {
      s0_error(l->diag, d->params[i].loc, "parameter '%s' is a %s; S0 parameters are numbers and pointers",
               d->params[i].name, type_name(l, t));
      ok = false;
    }
    for (j = 0; j < i; j++) {
      if (strcmp(d->params[j].name, d->params[i].name) == 0) {
        s0_error(l->diag, d->params[i].loc, "two parameters are named '%s'", d->params[i].name);
        ok = false;
      }
    }
    params[i].name = d->params[i].name;
    params[i].type = t;
  }
  if (name_taken(l, d->name, d->loc, false) || !ok) {
    return;
  }
  f = (Func *)cs_arena_new(l->arena, sizeof(Func));
  f->name = d->name;
  f->decl = d;
  f->mir = mir_func_add(l->m, d->name, params, d->nparams, ret, d->loc.line);
  f->mir->is_extern = d->kind == DK_EXTERN_FUNC;
  /* the entry point is the one symbol the C runtime must see */
  f->mir->is_export = d->kind == DK_FUNC && strcmp(d->name, "main") == 0;
  f->next = l->funcs;
  l->funcs = f;
}

static void lower_func(L *l, S0Decl *d) {
  Func *f = find_func(l, d->name);
  size_t i;
  if (f == NULL || f->decl != d) {
    return; /* its declaration failed, or it is a duplicate */
  }
  l->fn = f->mir;
  l->locals = NULL;
  l->loops = 0;
  for (i = 0; i < d->nparams; i++) {
    Local *v;
    if (name_taken(l, d->params[i].name, d->params[i].loc, false)) {
      continue;
    }
    v = (Local *)cs_arena_new(l->arena, sizeof(Local));
    v->name = d->params[i].name;
    v->type = f->mir->params[i].type;
    v->prev = l->locals;
    l->locals = v;
  }
  f->mir->body = lower_block(l, d->body);
  l->fn = NULL;
  l->locals = NULL;
}

MirModule *s0_lower(CsArena *arena, S0Diag *diag, S0Decl *decls) {
  L l;
  S0Decl *d;
  memset(&l, 0, sizeof l);
  l.arena = arena;
  l.diag = diag;
  l.m = mir_module_new();

  declare_structs(&l, decls);
  for (d = decls; d != NULL; d = d->next) {
    if (d->kind == DK_CONST) {
      declare_const(&l, d);
    }
  }
  for (d = decls; d != NULL; d = d->next) {
    if (d->kind == DK_GLOBAL) {
      declare_global(&l, d);
    }
  }
  for (d = decls; d != NULL; d = d->next) {
    if (d->kind == DK_FUNC || d->kind == DK_EXTERN_FUNC) {
      declare_func(&l, d);
    }
  }
  for (d = decls; d != NULL; d = d->next) {
    if (d->kind == DK_FUNC) {
      lower_func(&l, d);
    }
  }
  if (l.uses_alloc && diag->errors == 0) {
    if (find_func(&l, "cs_arena_alloc") != NULL) {
      S0Loc loc = decls->loc;
      s0_error(diag, loc, "'cs_arena_alloc' is the runtime function behind alloc and cannot be declared");
    } else {
      MirType *uword = mir_prim(l.m, MT_UWORD);
      MirParam *params = (MirParam *)cs_arena_new(&l.m->arena, 2 * sizeof(MirParam));
      MirFunc *f;
      params[0].type = uword;
      params[1].type = uword;
      f = mir_func_add(l.m, "cs_arena_alloc", params, 2, mir_ptr(l.m, mir_prim(l.m, MT_U8)), 0);
      f->is_extern = true;
    }
  }
  if (diag->errors != 0) {
    mir_module_free(l.m);
    return NULL;
  }
  return l.m;
}
