/* S0 expressions and their core MIR (s0_lower.c has the rules). */
#include "s0_lower.h"

bool is_untyped(const S0Expr *e) {
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
      s0_error(l->diag, loc, "E0401", "a %s is not a value; use one of its %s", type_name(l, t),
               t->kind == MT_ARRAY ? "elements" : "fields");
      return bad_val();
    }
    v = mir_unary(l->m, ME_LOAD, v, loc.line);
  }
  if (w.type != NULL) {
    if (!mir_type_eq(t, w.type)) {
      if (mir_type_is_int(t) && mir_type_is_int(w.type)) {
        s0_error(l->diag, loc, "E0403", "expected %s, found %s; convert it with 'as'", type_name(l, w.type),
                 type_name(l, t));
      } else {
        s0_error(l->diag, loc, "E0402", "expected %s, found %s", type_name(l, w.type), type_name(l, t));
      }
      return bad_val();
    }
  } else if (w.ptr_depth > 0) {
    if (!(t->kind == MT_PTR || (w.or_uword && t->kind == MT_UWORD))) {
      s0_error(l->diag, loc, "E0404", "expected a pointer%s, found %s", w.or_uword ? " or a uword" : "",
               type_name(l, t));
      return bad_val();
    }
  } else if (!mir_type_is_int(t)) {
    s0_error(l->diag, loc, "E0405", "expected a number, found %s", type_name(l, t));
    return bad_val();
  }
  return make_val(v, t);
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
      s0_error(l->diag, e->loc, "E0406",
               "the type of these literals is not determined by their context; write 'as T' after one");
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
      s0_error(l->diag, e->loc, "E0407", "the operands are %s and %s; convert one with 'as'", type_name(l, a->t),
               type_name(l, b->t));
      return false;
    }
  }
  return a->ok && b->ok;
}

Val lower_string(L *l, S0Expr *e) {
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

Val lower_call(L *l, S0Expr *e, Want w) {
  Func *f = find_func(l, e->name);
  MirExpr **args;
  size_t i;
  bool ok = true;
  if (strcmp(e->name, "alloc") == 0) {
    s0_error(l->diag, e->loc, "E0507",
             "alloc(count) can only initialize or be assigned to a pointer with ':='");
    return bad_val();
  }
  if (f == NULL) {
    s0_error(l->diag, e->loc, "E0305", "unknown function '%s'", e->name);
    return bad_val();
  }
  if (e->nargs != f->mir->nparams) {
    s0_error(l->diag, e->loc, "E0601", "'%s' takes %lu arguments, not %lu", e->name, (unsigned long)f->mir->nparams,
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
      s0_error(l->diag, e->loc, "E0602", "'%s' returns nothing", e->name);
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
        s0_error(l->diag, e->loc, "E0408", "an address converts to uword only, not to %s", type_name(l, to));
        return bad_val();
      }
      v = lower_expr(l, e->a, ow);
    } else {
      v = lower_expr(l, e->a, want_value());
    }
  } else {
    s0_error(l->diag, e->loc, "E0409", "'as' converts to a number or a pointer type, not to %s",
             type_name(l, to));
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

Val lower_expr(L *l, S0Expr *e, Want w) {
  switch (e->kind) {
  case EX_INT:
    if (w.type == NULL || !mir_type_is_int(w.type)) {
      if (w.type != NULL) {
        s0_error(l->diag, e->loc, "E0410", "expected %s, found a number", type_name(l, w.type));
      } else {
        s0_error(l->diag, e->loc, "E0411",
                 "the type of this literal is not determined by its context; write 'as T' after it");
      }
      return bad_val();
    }
    if (!literal_fits(w.type, e->ival)) {
      s0_error(l->diag, e->loc, "E0412", "%" PRIu64 " does not fit %s", e->ival, type_name(l, w.type));
      return bad_val();
    }
    return make_val(mir_const(l->m, w.type, e->ival, e->loc.line), w.type);

  case EX_STRING: {
    Val v = lower_string(l, e);
    if (w.type == NULL && w.ptr_depth == 0) {
      s0_error(l->diag, e->loc, "E0413", "a string is a #u8, not a number");
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
      s0_error(l->diag, e->loc, "E0414", "'%s' is a %s, not a value; use one of its %s", e->name, type_name(l, p.t),
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
      s0_error(l->diag, e->loc, "E0401", "a %s is not a value; use one of its %s", type_name(l, p.t),
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
      s0_error(l->diag, e->loc, "E0508",
               "'#' and '~' take the address of a variable, an element or a field");
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
        s0_error(l->diag, e->loc, "E0402", "expected %s, found %s", type_name(l, w.type), type_name(l, t));
        return bad_val();
      }
    } else if (w.ptr_depth == 0) {
      s0_error(l->diag, e->loc, "E0415",
               "an address is not a number; a pointer is expected only where one is declared");
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
    s0_error(l->diag, e->loc, "E0509", "alloc takes one argument, the element count");
    return bad_val();
  }
  if (to->elem->kind == MT_VOID) {
    s0_error(l->diag, e->loc, "E0510", "alloc needs an element type; the pointer is #void");
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
Val lower_pointer_value(L *l, S0Expr *e, S0Tok op, MirType *to) {
  if (is_alloc(e)) {
    if (op != TK_OWN) {
      s0_error(l->diag, e->loc, "E0511", "alloc returns ownership; receive it with ':='");
      return bad_val();
    }
    return lower_alloc(l, e, to);
  }
  return lower_expr(l, e, want_type(to));
}
