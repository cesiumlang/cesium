/* S0 declarations, and the entry point of the lowering, s0_lower(). */
#include "s0_lower.h"

static void declare_structs(L *l, S0Decl *decls) {
  S0Decl *d;
  /* names first, so fields can point at structs declared later */
  for (d = decls; d != NULL; d = d->next) {
    if (d->kind == DK_STRUCT) {
      StructName *s;
      if (find_struct(l, d->name) != NULL) {
        s0_error(l->diag, d->loc, "E0307", "struct '%s' is declared twice", d->name);
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
      s0_error(l->diag, d->loc, "E0308", "struct '%s' has no fields", d->name);
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
          s0_error(l->diag, f->loc, "E0306", "an array has at least one element");
          ok = false;
          continue;
        }
        t = mir_array(l->m, t, f->count);
      }
      for (j = 0; j < i; j++) {
        if (strcmp(fields[j].name, f->name) == 0) {
          s0_error(l->diag, f->loc, "E0309", "struct '%s' has two fields named '%s'", d->name, f->name);
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
    s0_error(l->diag, d->loc, "E0310", "a constant is a number in S0, not a %s", type_name(l, t));
    return;
  }
  if (d->init->kind == EX_INT) {
    value = d->init->ival;
    if (!literal_fits(t, value)) {
      s0_error(l->diag, d->init->loc, "E0412", "%" PRIu64 " does not fit %s", value, type_name(l, t));
      return;
    }
  } else if (d->init->kind == EX_NAME && find_const(l, d->init->name) != NULL) {
    Const *other = find_const(l, d->init->name);
    if (!mir_type_eq(other->type, t)) {
      s0_error(l->diag, d->init->loc, "E0311", "'%s' is a %s, not a %s", other->name, type_name(l, other->type),
               type_name(l, t));
      return;
    }
    value = other->value;
  } else {
    s0_error(l->diag, d->init->loc, "E0312",
             "a constant is initialized with a literal or another constant in S0");
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
      s0_error(l->diag, d->loc, "E0306", "an array has at least one element");
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
      s0_error(l->diag, d->loc, "E0514", "%s is only for pointers; a plain value is initialized with '='",
               s0_tok_name(d->op));
    } else if (d->init->kind == EX_INT && literal_fits(t, d->init->ival)) {
      g->init = MI_INT;
      g->ival = d->init->ival;
    } else if (c != NULL && mir_type_eq(c->type, t)) {
      g->init = MI_INT;
      g->ival = c->value;
    } else {
      s0_error(l->diag, d->init->loc, "E0313",
               "a global is initialized with a literal or a constant of its type in S0");
    }
  } else if (t->kind == MT_PTR) {
    if (d->op == TK_ASSIGN) {
      s0_error(l->diag, d->loc, "E0513",
               "a pointer is initialized with ':=', '#=' or '~='; plain '=' assigns through it");
    } else if (d->init->kind == EX_STRING && t->elem->kind == MT_U8) {
      Val v = lower_string(l, d->init);
      /* lower_string made the bytes global and an `as(ptr<u8>, @str.N)` expression */
      g->init = MI_ADDR;
      g->addr_name = v.e->a->name;
    } else {
      s0_error(l->diag, d->init->loc, "E0314",
               "a global pointer is initialized with a string in S0, or not at all");
    }
  } else {
    s0_error(l->diag, d->loc, "E0315", "a %s has no initializer in S0", type_name(l, t));
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
    s0_error(l->diag, d->loc, "E0316", "'%s' returns a %s; S0 functions return a number, a pointer or nothing", d->name,
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
      s0_error(l->diag, d->params[i].loc, "E0317", "parameter '%s' is a %s; S0 parameters are numbers and pointers",
               d->params[i].name, type_name(l, t));
      ok = false;
    }
    for (j = 0; j < i; j++) {
      if (strcmp(d->params[j].name, d->params[i].name) == 0) {
        s0_error(l->diag, d->params[i].loc, "E0318", "two parameters are named '%s'", d->params[i].name);
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
      s0_error(diag, loc, "E0319",
               "'cs_arena_alloc' is the runtime function behind alloc and cannot be declared");
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
