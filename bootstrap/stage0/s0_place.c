/* S0 places, somewhere a value is stored: a local, an element, a field
 * (s0_lower.c has the rules). */
#include "s0_lower.h"

/* The value stored at a place: the local itself, or a load. */
SirExpr *place_value(L *l, Place p, int line) {
  if (p.local != NULL) {
    return sir_local_ref(l->m, p.local, line);
  }
  return sir_unary(l->m, ME_LOAD, p.addr, line);
}

Place lower_place(L *l, S0Expr *e) {
  Place p = bad_place();
  switch (e->kind) {
  case EX_NAME: {
    Local *v = find_local(l, e->name);
    Global *g;
    if (v != NULL) {
      p.ok = true;
      p.addr = sir_expr(l->m, ME_ADDR, e->loc.line);
      p.addr->name = v->name;
      p.t = v->type;
      p.local = v->name;
      return p;
    }
    g = find_global(l, e->name);
    if (g != NULL) {
      p.ok = true;
      p.addr = sir_global_ref(l->m, g->name, e->loc.line);
      p.t = g->type;
      return p;
    }
    if (find_const(l, e->name) != NULL) {
      s0_error(l->diag, e->loc, "E0501", "'%s' is a constant: it has no address and cannot be assigned",
               e->name);
    } else if (find_func(l, e->name) != NULL) {
      s0_error(l->diag, e->loc, "E0303", "'%s' is a function", e->name);
    } else if (!s0_habit_name(l->diag, e->loc, e->name)) {
      s0_error(l->diag, e->loc, "E0304", "unknown name '%s'", e->name);
    }
    return p;
  }

  case EX_INDEX: {
    Place base = lower_place(l, e->a);
    Val idx;
    SirType *elem;
    SirExpr *first;
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
      first = sir_as(l->m, sir_ptr(l->m, elem), base.addr, e->loc.line);
    } else {
      s0_error(l->diag, e->loc, "E0502", "a %s cannot be indexed", type_name(l, base.t));
      return p;
    }
    if (elem->kind == MT_VOID) {
      s0_error(l->diag, e->loc, "E0503", "a pointer to void cannot be indexed");
      return p;
    }
    if (is_untyped(e->b)) {
      idx = lower_expr(l, e->b, want_type(sir_prim(l->m, MT_UWORD)));
    } else {
      idx = lower_expr(l, e->b, want_value());
    }
    if (!idx.ok) {
      return p;
    }
    p.ok = true;
    p.addr = sir_binary(l->m, ME_INDEX, first, idx.e, e->loc.line);
    p.t = elem;
    return p;
  }

  case EX_FIELD: {
    Place base = lower_place(l, e->a);
    size_t i;
    SirStruct *st;
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
      s0_error(l->diag, e->loc, "E0504", "a %s has no fields", type_name(l, base.t));
      return p;
    }
    st = base.t->st;
    for (i = 0; i < st->nfields; i++) {
      if (strcmp(st->fields[i].name, e->name) == 0) {
        p.ok = true;
        p.addr = sir_field(l->m, base.addr, e->name, e->loc.line);
        p.t = st->fields[i].type;
        return p;
      }
    }
    s0_error(l->diag, e->loc, "E0505", "struct %s has no field '%s'", st->name, e->name);
    return p;
  }

  default:
    s0_error(l->diag, e->loc, "E0506", "this expression is not a variable, an element or a field");
    return p;
  }
}

bool is_place_expr(const S0Expr *e) {
  return e->kind == EX_NAME || e->kind == EX_INDEX || e->kind == EX_FIELD;
}
