/* Core MIR verifier: resolves names, computes the type of every expression
 * and checks the rules of cesiumlang/cesium#8:
 *
 *   - every struct is defined once and does not contain itself by value;
 *   - global and function names are unique, local names unique per block;
 *   - operands have the types their operation needs (no implicit conversion);
 *   - break and continue appear only inside a loop;
 *   - a non-void function returns on every path;
 *   - v0: parameters, return values and locals read or assigned as values
 *     are scalars (integers and pointers); no floating-point operations.
 */
#include "mir.h"

typedef struct Scope {
  MirLocal *local;
  int depth;
  struct Scope *prev;
} Scope;

typedef struct Verifier {
  MirModule *m;
  MirFunc *fn;
  Scope *scope;
  int depth;
  int loops;
  MirLocal *locals_tail;
} Verifier;

static const char *type_text(Verifier *v, const MirType *t) {
  CsBuf b = {NULL, 0, 0};
  const char *s;
  mir_type_print(&b, t);
  s = cs_arena_str(&v->m->arena, b.data);
  cs_buf_free(&b);
  return s;
}

static MirFunc *find_func(MirModule *m, const char *name) {
  MirFunc *f;
  for (f = m->funcs; f != NULL; f = f->next) {
    if (strcmp(f->name, name) == 0) {
      return f;
    }
  }
  return NULL;
}

static MirGlobal *find_global(MirModule *m, const char *name) {
  MirGlobal *g;
  for (g = m->globals; g != NULL; g = g->next) {
    if (strcmp(g->name, name) == 0) {
      return g;
    }
  }
  return NULL;
}

static MirLocal *find_local(Verifier *v, const char *name) {
  Scope *s;
  for (s = v->scope; s != NULL; s = s->prev) {
    if (strcmp(s->local->name, name) == 0) {
      return s->local;
    }
  }
  return NULL;
}

static MirLocal *declare_local(Verifier *v, const char *name, MirType *type, bool is_param, int line) {
  Scope *s;
  MirLocal *l;
  for (s = v->scope; s != NULL && s->depth == v->depth; s = s->prev) {
    if (strcmp(s->local->name, name) == 0) {
      mir_error(v->m, line, "%%%s is declared twice in the same block", name);
      return s->local;
    }
  }
  l = (MirLocal *)cs_arena_new(&v->m->arena, sizeof(MirLocal));
  l->name = name;
  l->type = type;
  l->is_param = is_param;
  l->slot = v->fn->nslots++;
  if (v->locals_tail != NULL) {
    v->locals_tail->next = l;
  } else {
    v->fn->locals = l;
  }
  v->locals_tail = l;
  s = (Scope *)cs_arena_new(&v->m->arena, sizeof(Scope));
  s->local = l;
  s->depth = v->depth;
  s->prev = v->scope;
  v->scope = s;
  return l;
}

/* A type a value can have: every struct in it is defined, and no array has
 * a void element. `what` names the use for the message. */
static bool check_type(Verifier *v, const MirType *t, int line, const char *what) {
  switch (t->kind) {
  case MT_PTR:
    /* a pointer may point at an undefined struct only if it is defined
     * somewhere in the module; that is checked once for all structs */
    return t->elem->kind == MT_VOID || t->elem->kind == MT_STRUCT || check_type(v, t->elem, line, what);
  case MT_ARRAY:
    if (t->elem->kind == MT_VOID) {
      mir_error(v->m, line, "%s: an array of void", what);
      return false;
    }
    return check_type(v, t->elem, line, what);
  case MT_STRUCT:
    if (!t->st->defined) {
      mir_error(v->m, line, "%s: struct %s is not defined", what, t->st->name);
      return false;
    }
    return true;
  default:
    return true;
  }
}

static bool no_floats(Verifier *v, const MirType *t, int line) {
  if (t->kind == MT_F32 || t->kind == MT_F64) {
    mir_error(v->m, line, "floating-point types are reserved in MIR version %d", MIR_VERSION);
    return false;
  }
  return true;
}

/* ---- expressions ------------------------------------------------------- */

static bool verify_expr(Verifier *v, MirExpr *e);

/* Verifies e and requires an integer result. */
static bool verify_int(Verifier *v, MirExpr *e, const char *what) {
  if (!verify_expr(v, e)) {
    return false;
  }
  if (!mir_type_is_int(e->type)) {
    mir_error(v->m, e->line, "%s must be an integer, not %s", what, type_text(v, e->type));
    return false;
  }
  return true;
}

static bool verify_expr(Verifier *v, MirExpr *e) {
  MirModule *m = v->m;
  size_t i;
  if (m->failed) {
    return false;
  }
  switch (e->op) {
  case ME_CONST:
    if (!mir_type_is_int(e->type)) {
      mir_error(m, e->line, "a constant needs an integer type");
      return false;
    }
    return true;

  case ME_LOCAL:
    e->local = find_local(v, e->name);
    if (e->local == NULL) {
      mir_error(m, e->line, "unknown local %%%s", e->name);
      return false;
    }
    if (!mir_type_is_scalar(e->local->type)) {
      mir_error(m, e->line, "%%%s has type %s and cannot be read as a value; use addr(%%%s)", e->name,
                type_text(v, e->local->type), e->name);
      return false;
    }
    e->type = e->local->type;
    return true;

  case ME_GLOBAL:
    e->global = find_global(m, e->name);
    if (e->global == NULL) {
      if (find_func(m, e->name) != NULL) {
        mir_error(m, e->line, "@%s is a function; function addresses are not values in MIR version %d", e->name,
                  MIR_VERSION);
      } else {
        mir_error(m, e->line, "unknown global @%s", e->name);
      }
      return false;
    }
    e->type = mir_ptr(m, e->global->type);
    return true;

  case ME_ADDR:
    e->local = find_local(v, e->name);
    if (e->local == NULL) {
      mir_error(m, e->line, "unknown local %%%s", e->name);
      return false;
    }
    e->type = mir_ptr(m, e->local->type);
    return true;

  case ME_LOAD:
    if (!verify_expr(v, e->a)) {
      return false;
    }
    if (e->a->type->kind != MT_PTR) {
      mir_error(m, e->line, "load needs a pointer, not %s", type_text(v, e->a->type));
      return false;
    }
    if (!mir_type_is_scalar(e->a->type->elem)) {
      mir_error(m, e->line, "load of %s: only integers and pointers are loaded", type_text(v, e->a->type->elem));
      return false;
    }
    e->type = e->a->type->elem;
    return true;

  case ME_INDEX:
    if (!verify_expr(v, e->a) || !verify_int(v, e->b, "an index")) {
      return false;
    }
    if (e->a->type->kind != MT_PTR || e->a->type->elem->kind == MT_VOID) {
      mir_error(m, e->line, "index needs a pointer to a sized type, not %s", type_text(v, e->a->type));
      return false;
    }
    if (!check_type(v, e->a->type->elem, e->line, "index")) {
      return false;
    }
    e->type = e->a->type;
    return true;

  case ME_FIELD: {
    MirStruct *st;
    if (!verify_expr(v, e->a)) {
      return false;
    }
    if (e->a->type->kind != MT_PTR || e->a->type->elem->kind != MT_STRUCT) {
      mir_error(m, e->line, "field needs a pointer to a struct, not %s", type_text(v, e->a->type));
      return false;
    }
    st = e->a->type->elem->st;
    for (i = 0; i < st->nfields; i++) {
      if (strcmp(st->fields[i].name, e->name) == 0) {
        e->field_index = i;
        e->type = mir_ptr(m, st->fields[i].type);
        return true;
      }
    }
    mir_error(m, e->line, "struct %s has no field %s", st->name, e->name);
    return false;
  }

  case ME_CALL:
    e->callee = find_func(m, e->name);
    if (e->callee == NULL) {
      mir_error(m, e->line, "unknown function @%s", e->name);
      return false;
    }
    if (e->nargs != e->callee->nparams) {
      mir_error(m, e->line, "@%s takes %lu arguments, not %lu", e->name, (unsigned long)e->callee->nparams,
                (unsigned long)e->nargs);
      return false;
    }
    for (i = 0; i < e->nargs; i++) {
      if (!verify_expr(v, e->args[i])) {
        return false;
      }
      if (!mir_type_eq(e->args[i]->type, e->callee->params[i].type)) {
        mir_error(m, e->args[i]->line, "argument %lu of @%s is %s, not %s", (unsigned long)(i + 1), e->name,
                  type_text(v, e->args[i]->type), type_text(v, e->callee->params[i].type));
        return false;
      }
    }
    e->type = e->callee->ret;
    return true;

  case ME_ADD:
  case ME_SUB:
  case ME_MUL:
  case ME_DIV:
  case ME_REM:
  case ME_AND:
  case ME_OR:
  case ME_XOR:
  case ME_SHL:
  case ME_SHR:
    if (!verify_int(v, e->a, "an operand") || !verify_int(v, e->b, "an operand")) {
      return false;
    }
    if (!mir_type_eq(e->a->type, e->b->type)) {
      mir_error(m, e->line, "%s of %s and %s: the operands must have the same type", mir_op_name(e->op),
                type_text(v, e->a->type), type_text(v, e->b->type));
      return false;
    }
    e->type = e->a->type;
    return true;

  case ME_NOT:
  case ME_NEG:
    if (!verify_int(v, e->a, "an operand")) {
      return false;
    }
    e->type = e->a->type;
    return true;

  case ME_EQ:
  case ME_NE:
  case ME_LT:
  case ME_LE:
  case ME_GT:
  case ME_GE:
    if (!verify_expr(v, e->a) || !verify_expr(v, e->b)) {
      return false;
    }
    if (!mir_type_is_scalar(e->a->type) || !mir_type_eq(e->a->type, e->b->type)) {
      mir_error(m, e->line, "%s of %s and %s: the operands must be integers or pointers of the same type",
                mir_op_name(e->op), type_text(v, e->a->type), type_text(v, e->b->type));
      return false;
    }
    e->type = mir_prim(m, MT_U8);
    return true;

  case ME_LAND:
  case ME_LOR:
    if (!verify_int(v, e->a, "an operand") || !verify_int(v, e->b, "an operand")) {
      return false;
    }
    e->type = mir_prim(m, MT_U8);
    return true;

  case ME_LNOT:
    if (!verify_int(v, e->a, "an operand")) {
      return false;
    }
    e->type = mir_prim(m, MT_U8);
    return true;

  case ME_AS: {
    const MirType *from;
    const MirType *to = e->targ;
    bool ok;
    if (!verify_expr(v, e->a) || !no_floats(v, to, e->line)) {
      return false;
    }
    from = e->a->type;
    ok = (mir_type_is_int(from) && mir_type_is_int(to)) || (from->kind == MT_PTR && to->kind == MT_PTR) ||
         (from->kind == MT_PTR && to->kind == MT_UWORD) || (from->kind == MT_UWORD && to->kind == MT_PTR);
    if (!ok) {
      mir_error(m, e->line, "no conversion from %s to %s", type_text(v, from), type_text(v, to));
      return false;
    }
    e->type = e->targ;
    return true;
  }

  case ME_SIZEOF:
  case ME_ALIGNOF:
    if (e->targ->kind == MT_VOID) {
      mir_error(m, e->line, "%s of void", mir_op_name(e->op));
      return false;
    }
    if (!check_type(v, e->targ, e->line, mir_op_name(e->op))) {
      return false;
    }
    e->type = mir_prim(m, MT_UWORD);
    return true;

  case ME_OP_COUNT:
    break;
  }
  CS_BUG("unknown operation");
  return false;
}

/* ---- statements -------------------------------------------------------- */

static void verify_block(Verifier *v, MirStmt *s);

static void verify_stmt(Verifier *v, MirStmt *s) {
  MirModule *m = v->m;
  if (m->failed) {
    return;
  }
  switch (s->kind) {
  case MS_LOCAL:
    if (s->type->kind == MT_VOID) {
      mir_error(m, s->line, "%%%s cannot have type void", s->name);
      return;
    }
    if (!check_type(v, s->type, s->line, "local") || !no_floats(v, s->type, s->line)) {
      return;
    }
    if (s->a != NULL) {
      /* the initializer is evaluated before the name comes into scope */
      if (!verify_expr(v, s->a)) {
        return;
      }
      if (!mir_type_eq(s->a->type, s->type)) {
        mir_error(m, s->line, "%%%s is %s but its initializer is %s", s->name, type_text(v, s->type),
                  type_text(v, s->a->type));
        return;
      }
    }
    s->local = declare_local(v, s->name, s->type, false, s->line);
    return;

  case MS_SET:
    s->local = find_local(v, s->name);
    if (s->local == NULL) {
      mir_error(m, s->line, "unknown local %%%s", s->name);
      return;
    }
    if (!verify_expr(v, s->a)) {
      return;
    }
    if (!mir_type_is_scalar(s->local->type) || !mir_type_eq(s->a->type, s->local->type)) {
      mir_error(m, s->line, "set %%%s: the local is %s and the value is %s", s->name, type_text(v, s->local->type),
                type_text(v, s->a->type));
    }
    return;

  case MS_STORE:
    if (!verify_expr(v, s->a) || !verify_expr(v, s->b)) {
      return;
    }
    if (s->a->type->kind != MT_PTR || !mir_type_is_scalar(s->b->type) || !mir_type_eq(s->a->type->elem, s->b->type)) {
      mir_error(m, s->line, "store of %s through %s", type_text(v, s->b->type), type_text(v, s->a->type));
    }
    return;

  case MS_EVAL:
    verify_expr(v, s->a);
    return;

  case MS_IF:
    if (!verify_int(v, s->a, "a condition")) {
      return;
    }
    verify_block(v, s->body);
    verify_block(v, s->els);
    return;

  case MS_WHILE:
    if (!verify_int(v, s->a, "a condition")) {
      return;
    }
    v->loops++;
    verify_block(v, s->body);
    v->loops--;
    return;

  case MS_BREAK:
  case MS_CONTINUE:
    if (v->loops == 0) {
      mir_error(m, s->line, "%s outside a loop", s->kind == MS_BREAK ? "break" : "continue");
    }
    return;

  case MS_RETURN:
    if (s->a == NULL) {
      if (v->fn->ret->kind != MT_VOID) {
        mir_error(m, s->line, "@%s must return a %s", v->fn->name, type_text(v, v->fn->ret));
      }
      return;
    }
    if (!verify_expr(v, s->a)) {
      return;
    }
    if (v->fn->ret->kind == MT_VOID || !mir_type_eq(s->a->type, v->fn->ret)) {
      mir_error(m, s->line, "@%s returns %s, not %s", v->fn->name, type_text(v, v->fn->ret), type_text(v, s->a->type));
    }
    return;
  }
  CS_BUG("unknown statement");
}

static void verify_block(Verifier *v, MirStmt *s) {
  Scope *saved = v->scope;
  v->depth++;
  for (; s != NULL; s = s->next) {
    verify_stmt(v, s);
  }
  v->depth--;
  v->scope = saved;
}

/* True when control cannot reach the end of the block. A loop is never
 * counted as returning: `while 1:u8 { ... }` still needs a return after it. */
static bool block_returns(const MirStmt *s) {
  for (; s != NULL; s = s->next) {
    if (s->kind == MS_RETURN) {
      return true;
    }
    if (s->kind == MS_IF && s->els != NULL && block_returns(s->body) && block_returns(s->els)) {
      return true;
    }
  }
  return false;
}

/* ---- declarations ------------------------------------------------------ */

static bool struct_contains(const MirType *t, const MirStruct *target, int depth) {
  size_t i;
  if (depth > 64) {
    return true;
  }
  if (t->kind == MT_ARRAY) {
    return struct_contains(t->elem, target, depth + 1);
  }
  if (t->kind != MT_STRUCT) {
    return false;
  }
  if (t->st == target) {
    return true;
  }
  for (i = 0; i < t->st->nfields; i++) {
    if (struct_contains(t->st->fields[i].type, target, depth + 1)) {
      return true;
    }
  }
  return false;
}

static void verify_signature(Verifier *v, MirFunc *f) {
  MirModule *m = v->m;
  size_t i;
  size_t j;
  if (f->ret->kind != MT_VOID && !mir_type_is_scalar(f->ret)) {
    mir_error(m, f->line, "@%s returns %s: only integers, pointers and void in MIR version %d", f->name,
              type_text(v, f->ret), MIR_VERSION);
    return;
  }
  for (i = 0; i < f->nparams; i++) {
    if (!mir_type_is_scalar(f->params[i].type)) {
      mir_error(m, f->line, "parameter %lu of @%s is %s: only integers and pointers in MIR version %d",
                (unsigned long)(i + 1), f->name, type_text(v, f->params[i].type), MIR_VERSION);
      return;
    }
    for (j = 0; j < i && !f->is_extern; j++) {
      if (strcmp(f->params[i].name, f->params[j].name) == 0) {
        mir_error(m, f->line, "@%s has two parameters named %%%s", f->name, f->params[i].name);
        return;
      }
    }
  }
}

bool mir_verify(MirModule *m) {
  Verifier v;
  MirStruct *s;
  MirGlobal *g;
  MirGlobal *g2;
  MirFunc *f;
  MirFunc *f2;
  size_t i;
  size_t j;
  memset(&v, 0, sizeof v);
  v.m = m;
  if (m->failed) {
    return false;
  }

  for (s = m->structs; s != NULL && !m->failed; s = s->next) {
    if (!s->defined) {
      mir_error(m, 0, "struct %s is used but never defined", s->name);
      break;
    }
    for (i = 0; i < s->nfields && !m->failed; i++) {
      MirType *ft = s->fields[i].type;
      if (ft->kind == MT_VOID) {
        mir_error(m, s->line, "field %s of struct %s has type void", s->fields[i].name, s->name);
      } else if (check_type(&v, ft, s->line, "field") && no_floats(&v, ft, s->line) && struct_contains(ft, s, 0)) {
        mir_error(m, s->line, "struct %s contains itself through field %s", s->name, s->fields[i].name);
      }
      for (j = 0; j < i && !m->failed; j++) {
        if (strcmp(s->fields[i].name, s->fields[j].name) == 0) {
          mir_error(m, s->line, "struct %s has two fields named %s", s->name, s->fields[i].name);
        }
      }
    }
  }

  for (g = m->globals; g != NULL && !m->failed; g = g->next) {
    for (g2 = m->globals; g2 != g; g2 = g2->next) {
      if (strcmp(g->name, g2->name) == 0) {
        mir_error(m, g->line, "@%s is defined twice", g->name);
        break;
      }
    }
    if (m->failed) {
      break;
    }
    if (g->type->kind == MT_VOID) {
      mir_error(m, g->line, "@%s cannot have type void", g->name);
      break;
    }
    if (!check_type(&v, g->type, g->line, "global") || !no_floats(&v, g->type, g->line)) {
      break;
    }
    switch (g->init) {
    case MI_ZERO:
      break;
    case MI_INT:
      if (!mir_type_is_int(g->type)) {
        mir_error(m, g->line, "@%s is not an integer but has an integer initializer", g->name);
      }
      break;
    case MI_BYTES:
      if (g->type->kind != MT_ARRAY || g->type->elem->kind != MT_U8 || g->type->count != g->nbytes) {
        mir_error(m, g->line, "@%s: a string of %lu bytes needs the type [%lu x u8]", g->name,
                  (unsigned long)g->nbytes, (unsigned long)g->nbytes);
      }
      break;
    case MI_ADDR: {
      MirGlobal *target = find_global(m, g->addr_name);
      if (target == NULL) {
        mir_error(m, g->line, "@%s: unknown global @%s", g->name, g->addr_name);
      } else if (g->type->kind != MT_PTR) {
        mir_error(m, g->line, "@%s is not a pointer but is initialized with an address", g->name);
      }
      break;
    }
    }
  }

  for (f = m->funcs; f != NULL && !m->failed; f = f->next) {
    for (f2 = m->funcs; f2 != f; f2 = f2->next) {
      if (strcmp(f->name, f2->name) == 0) {
        mir_error(m, f->line, "@%s is defined twice", f->name);
        break;
      }
    }
    if (!m->failed && find_global(m, f->name) != NULL) {
      mir_error(m, f->line, "@%s names both a function and a global", f->name);
    }
    if (!m->failed) {
      verify_signature(&v, f);
    }
  }

  for (f = m->funcs; f != NULL && !m->failed; f = f->next) {
    if (f->is_extern) {
      continue;
    }
    v.fn = f;
    v.scope = NULL;
    v.depth = 0;
    v.loops = 0;
    v.locals_tail = NULL;
    f->locals = NULL;
    f->nslots = 0;
    for (i = 0; i < f->nparams; i++) {
      declare_local(&v, f->params[i].name, f->params[i].type, true, f->line);
    }
    verify_block(&v, f->body);
    if (!m->failed && f->ret->kind != MT_VOID && !block_returns(f->body)) {
      mir_error(m, f->line, "@%s does not return a value on every path", f->name);
    }
  }

  m->verified = !m->failed;
  return m->verified;
}
