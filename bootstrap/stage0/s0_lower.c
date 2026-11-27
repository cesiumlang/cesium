/* S0 checking and lowering to core SIR, following the "S0 -> core SIR
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
#include "s0_lower.h"

Val bad_val(void) {
  Val v;
  v.ok = false;
  v.e = NULL;
  v.t = NULL;
  return v;
}

Val make_val(SirExpr *e, SirType *t) {
  Val v;
  v.ok = true;
  v.e = e;
  v.t = t;
  return v;
}

Place bad_place(void) {
  Place p;
  p.ok = false;
  p.addr = NULL;
  p.t = NULL;
  p.local = NULL;
  return p;
}

/* ---- types ------------------------------------------------------------- */

int depth_of(const SirType *t) {
  int d = 0;
  while (t->kind == MT_PTR) {
    d++;
    t = t->elem;
  }
  return d;
}

/* A type as S0 writes it, for messages. */
static void type_put(CsBuf *b, const SirType *t) {
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
    sir_type_print(b, t);
    break;
  }
}

const char *type_name(L *l, const SirType *t) {
  CsBuf b = {NULL, 0, 0};
  const char *s;
  type_put(&b, t);
  s = cs_arena_str(l->arena, b.data);
  cs_buf_free(&b);
  return s;
}

StructName *find_struct(L *l, const char *name) {
  StructName *s;
  for (s = l->structs; s != NULL; s = s->next) {
    if (strcmp(s->name, name) == 0) {
      return s;
    }
  }
  return NULL;
}

/* The SIR type of a written type, or NULL after an error. */
SirType *resolve_type(L *l, const S0TypeSyn *syn) {
  SirType *t;
  int i;
  switch (syn->base) {
  case BT_VOID:
    t = sir_prim(l->m, MT_VOID);
    break;
  case BT_U8:
    t = sir_prim(l->m, MT_U8);
    break;
  case BT_I32:
    t = sir_prim(l->m, MT_I32);
    break;
  case BT_U32:
    t = sir_prim(l->m, MT_U32);
    break;
  case BT_I64:
    t = sir_prim(l->m, MT_I64);
    break;
  case BT_U64:
    t = sir_prim(l->m, MT_U64);
    break;
  case BT_UWORD:
    t = sir_prim(l->m, MT_UWORD);
    break;
  default:
    if (find_struct(l, syn->name) == NULL) {
      if (!s0_habit_type(l->diag, syn->loc, syn->name)) {
        s0_error(l->diag, syn->loc, "E0301", "unknown type '%s'", syn->name);
      }
      return NULL;
    }
    t = &sir_struct_ref(l->m, syn->name)->type;
    break;
  }
  for (i = 0; i < syn->depth; i++) {
    t = sir_ptr(l->m, t);
  }
  return t;
}

bool literal_fits(const SirType *t, uint64_t v) {
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

Local *find_local(L *l, const char *name) {
  Local *v;
  for (v = l->locals; v != NULL; v = v->prev) {
    if (strcmp(v->name, name) == 0) {
      return v;
    }
  }
  return NULL;
}

Const *find_const(L *l, const char *name) {
  Const *c;
  for (c = l->consts; c != NULL; c = c->next) {
    if (strcmp(c->name, name) == 0) {
      return c;
    }
  }
  return NULL;
}

Global *find_global(L *l, const char *name) {
  Global *g;
  for (g = l->globals; g != NULL; g = g->next) {
    if (strcmp(g->name, name) == 0) {
      return g;
    }
  }
  return NULL;
}

Func *find_func(L *l, const char *name) {
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
bool name_taken(L *l, const char *name, S0Loc loc, bool check_locals) {
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
    s0_error(l->diag, loc, "E0302", "'%s' is already the name of %s", name, what);
    return true;
  }
  return false;
}

Want want_value(void) {
  Want w;
  memset(&w, 0, sizeof w);
  return w;
}

Want want_type(SirType *t) {
  Want w;
  memset(&w, 0, sizeof w);
  w.type = t;
  return w;
}
