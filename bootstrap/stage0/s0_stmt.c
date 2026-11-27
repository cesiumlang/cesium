/* S0 statements and their core SIR (s0_lower.c has the rules). */
#include "s0_lower.h"

static void block_add(Block *b, SirStmt *s) {
  if (b->tail != NULL) {
    b->tail->next = s;
  } else {
    b->head = s;
  }
  b->tail = s;
}

/* A SIR statement located at the first token of the S0 statement. */
static SirStmt *new_stmt(L *l, SirStmtKind kind, const S0Stmt *s) {
  SirStmt *ms = sir_stmt(l->m, kind, s->loc.line);
  sir_stmt_loc(l->m, ms, s->loc.file, (uint32_t)s->loc.line, (uint32_t)s->loc.col);
  return ms;
}


static void lower_decl(L *l, S0Stmt *s, Block *out) {
  SirType *t = resolve_type(l, &s->type);
  SirStmt *ms;
  Local *v;
  Val init = bad_val();
  bool has_init = s->op != TK_EOF;
  if (t == NULL) {
    return;
  }
  if (s->is_array) {
    if (s->count == 0) {
      s0_error(l->diag, s->loc, "E0306", "an array has at least one element");
      return;
    }
    t = sir_array(l->m, t, s->count);
  }
  if (has_init) {
    if (!sir_type_is_scalar(t)) {
      s0_error(l->diag, s->loc, "E0512", "a %s has no initializer in S0; assign its %s one at a time",
               type_name(l, t), t->kind == MT_ARRAY ? "elements" : "fields");
      return;
    }
    if (t->kind == MT_PTR) {
      if (s->op == TK_ASSIGN) {
        s0_error(l->diag, s->loc, "E0513",
                 "a pointer is initialized with ':=', '#=' or '~='; plain '=' assigns through it");
        return;
      }
      init = lower_pointer_value(l, s->b, s->op, t);
    } else {
      if (s->op != TK_ASSIGN) {
        s0_error(l->diag, s->loc, "E0514", "%s is only for pointers; a plain value is initialized with '='",
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
  ms = new_stmt(l, MS_LOCAL, s);
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
  SirStmt *ms;
  if (!is_place_expr(s->a)) {
    s0_error(l->diag, s->loc, "E0515",
             "the left side of an assignment is a variable, an element or a field");
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
    if (!sir_type_is_int(p.t)) {
      s0_error(l->diag, s->loc, "E0516", "a %s is not assigned as a whole in S0; assign its %s", type_name(l, p.t),
               p.t->kind == MT_ARRAY ? "elements" : "fields");
      return;
    }
    v = lower_expr(l, s->b, want_type(p.t));
  } else {
    if (p.t->kind != MT_PTR) {
      s0_error(l->diag, s->loc, "E0517", "%s sets where a pointer points; the left side is a %s", s0_tok_name(s->op),
               type_name(l, p.t));
      return;
    }
    v = lower_pointer_value(l, s->b, s->op, p.t);
  }
  if (!v.ok) {
    return;
  }
  if (p.local != NULL) {
    ms = new_stmt(l, MS_SET, s);
    ms->name = p.local;
    ms->a = v.e;
  } else {
    ms = new_stmt(l, MS_STORE, s);
    ms->a = p.addr;
    ms->b = v.e;
  }
  block_add(out, ms);
}

static void lower_stmt(L *l, S0Stmt *s, Block *out) {
  SirStmt *ms;
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
      s0_error(l->diag, s->loc, "E0603", "only a call can be a statement");
      return;
    }
    w.allow_void = true;
    v = lower_call(l, s->a, w);
    if (!v.ok) {
      return;
    }
    ms = new_stmt(l, MS_EVAL, s);
    ms->a = v.e;
    block_add(out, ms);
    return;
  }

  case ST_IF:
    v = lower_expr(l, s->a, want_value());
    ms = new_stmt(l, MS_IF, s);
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
    ms = new_stmt(l, MS_WHILE, s);
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
      s0_error(l->diag, s->loc, "E0604", "'%s' outside a loop", s->kind == ST_BREAK ? "break" : "continue");
      return;
    }
    block_add(out, new_stmt(l, s->kind == ST_BREAK ? MS_BREAK : MS_CONTINUE, s));
    return;

  case ST_RETURN:
    ms = new_stmt(l, MS_RETURN, s);
    if (s->a == NULL) {
      if (l->fn->ret->kind != MT_VOID) {
        s0_error(l->diag, s->loc, "E0605", "'%s' returns a %s", l->fn->name, type_name(l, l->fn->ret));
        return;
      }
    } else {
      if (l->fn->ret->kind == MT_VOID) {
        s0_error(l->diag, s->loc, "E0602", "'%s' returns nothing", l->fn->name);
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

SirStmt *lower_block(L *l, S0Stmt *s) {
  Local *saved = l->locals;
  Block b = {NULL, NULL};
  for (; s != NULL; s = s->next) {
    lower_stmt(l, s, &b);
  }
  l->locals = saved;
  return b.head;
}
