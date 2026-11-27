/* Variables and lexical scopes for debuggers (cesiumlang/cesium#143).
 *
 * QBE promotes stack slots to registers and does not say where values go, so
 * a debug build keeps every local in memory, as clang -O0 does: at entry, the
 * address of each local's slot is stored to a symbol of its own, and QBE does
 * not promote a slot whose address escapes. Each block that declares locals
 * stores to a symbol where it opens and where it closes. The rewrite of the
 * assembly reads the slots' offsets and the scopes' places from these
 * markers (mir_codeview.c, mir_dwarf.c); this file numbers them the same way
 * for the lowering and the rewrite.
 */
#include "mir.h"

bool mir_block_is_scope(const MirStmt *body) {
  for (; body != NULL; body = body->next) {
    if (body->kind == MS_LOCAL) {
      return true;
    }
  }
  return false;
}

typedef struct Walk {
  MirDebugFunc *d;
  CsBuf parents; /* int per scope */
} Walk;

static void walk_block(Walk *w, const MirStmt *s, int scope);

/* A nested block: a scope of its own if it declares a local. */
static void walk_nested(Walk *w, const MirStmt *body, int scope) {
  if (mir_block_is_scope(body)) {
    int k = (int)(w->parents.len / sizeof(int));
    cs_buf_put(&w->parents, (const char *)&scope, sizeof scope);
    scope = k;
  }
  walk_block(w, body, scope);
}

static void walk_block(Walk *w, const MirStmt *s, int scope) {
  for (; s != NULL; s = s->next) {
    if (s->kind == MS_LOCAL) {
      size_t i;
      for (i = 0; i < w->d->nlocals; i++) {
        if (w->d->locals[i] == s->local) {
          w->d->scope[i] = scope;
        }
      }
    } else if (s->kind == MS_IF) {
      walk_nested(w, s->body, scope);
      walk_nested(w, s->els, scope);
    } else if (s->kind == MS_WHILE) {
      walk_nested(w, s->body, scope);
    }
  }
}

void mir_debug_func(CsArena *a, MirDebugFunc *d, const MirFunc *f) {
  Walk w;
  const MirLocal *l;
  int top = -1;
  size_t i = 0;
  memset(d, 0, sizeof *d);
  d->f = f;
  for (l = f->locals; l != NULL; l = l->next) {
    d->nlocals++;
  }
  d->locals = cs_arena_new(a, cs_mul_size(d->nlocals + 1, sizeof *d->locals));
  d->scope = cs_arena_new(a, cs_mul_size(d->nlocals + 1, sizeof *d->scope));
  for (l = f->locals; l != NULL; l = l->next) {
    d->locals[i++] = (MirLocal *)l;
  }
  w.d = d;
  w.parents.data = NULL;
  w.parents.len = 0;
  w.parents.cap = 0;
  cs_buf_put(&w.parents, (const char *)&top, sizeof top);
  walk_block(&w, f->body, 0);
  d->nscopes = w.parents.len / sizeof(int);
  d->parent = cs_arena_new(a, cs_mul_size(d->nscopes, sizeof *d->parent));
  memcpy(d->parent, w.parents.data, d->nscopes * sizeof(int));
  cs_buf_free(&w.parents);
}

void mir_vars_init(MirVars *v, const MirModule *m, const MirTarget *t) {
  memset(v, 0, sizeof *v);
  v->m = m;
  v->prefix = t != NULL && t->format == MIR_MACHO ? "L" : ".L";
  v->underscore = t != NULL && t->format == MIR_MACHO;
}

void mir_vars_free(MirVars *v) {
  size_t i;
  for (i = 0; i < v->funcs.len / sizeof(MirVarsFunc *); i++) {
    MirVarsFunc *c;
    memcpy(&c, v->funcs.data + i * sizeof(MirVarsFunc *), sizeof(MirVarsFunc *));
    if (c != NULL) {
      cs_buf_free(&c->traps);
    }
  }
  cs_buf_free(&v->funcs);
  cs_arena_free(&v->arena);
}

static const MirFunc *find_func(const MirModule *m, const char *name) {
  const MirFunc *f;
  for (f = m->funcs; f != NULL; f = f->next) {
    if (!f->is_extern && strcmp(f->name, name) == 0) {
      return f;
    }
  }
  return NULL;
}

void mir_vars_begin(MirVars *v, const char *label) {
  const MirFunc *f = NULL;
  MirVarsFunc *c;
  v->cur = NULL;
  if (v->m != NULL) {
    f = find_func(v->m, v->underscore && label[0] == '_' ? label + 1 : label);
  }
  if (f == NULL) {
    return;
  }
  c = cs_arena_new(&v->arena, sizeof *c);
  c->id = v->count++;
  mir_debug_func(&v->arena, &c->d, f);
  c->vars = cs_arena_new(&v->arena, cs_mul_size(c->d.nlocals + 1, sizeof *c->vars));
  c->opened = cs_arena_new(&v->arena, cs_mul_size(c->d.nscopes, sizeof *c->opened));
  c->closed = cs_arena_new(&v->arena, cs_mul_size(c->d.nscopes, sizeof *c->closed));
  mir_debug_scan_start(&v->scan, c->vars, c->d.nlocals);
  v->cur = c;
}

/* The end of the last trap's block, once it was opened. */
static void close_trap(MirVars *v, CsBuf *dst) {
  MirVarsFunc *c = v->cur;
  if (c != NULL && c->trap_state != 0) {
    cs_buf_printf(dst, "%scs_te%" PRIu32 "_%d:\n", v->prefix, c->id, (int)mir_vars_ntraps(c) - 1);
    c->trap_state = 0;
  }
}

void mir_vars_line(MirVars *v, CsBuf *dst, const char *line, size_t len) {
  MirVarsFunc *c = v->cur;
  int k = 0;
  char kind;
  if (c == NULL) {
    return;
  }
  if (c->trap_state == 2) {
    close_trap(v, dst);
  } else if (c->trap_state == 1 && mir_asm_is_trap(line, len)) {
    c->trap_state = 2;
  }
  kind = mir_debug_scan_line(&v->scan, line, len, &k);
  if (kind == 0 || k < 1 || (size_t)k >= c->d.nscopes) {
    return;
  }
  if (kind == 't') {
    /* AArch64 names the marker in two instructions */
    if (c->trap_state == 1) {
      return;
    }
    close_trap(v, dst);
    cs_buf_printf(dst, "%scs_tb%" PRIu32 "_%d:\n", v->prefix, c->id, (int)mir_vars_ntraps(c));
    cs_buf_put(&c->traps, (const char *)&k, sizeof k);
    c->trap_state = 1;
  } else if (kind == 'o' && !c->opened[k]) {
    cs_buf_printf(dst, "%scs_sb%" PRIu32 "_%d:\n", v->prefix, c->id, k);
    c->opened[k] = true;
  } else if (kind == 'c' && c->opened[k] && !c->closed[k]) {
    cs_buf_printf(dst, "%scs_se%" PRIu32 "_%d:\n", v->prefix, c->id, k);
    c->closed[k] = true;
  }
}

MirVarsFunc *mir_vars_end(MirVars *v, CsBuf *out, bool keep) {
  MirVarsFunc *c = v->cur;
  size_t k;
  close_trap(v, out);
  for (k = 1; c != NULL && k < c->d.nscopes; k++) {
    if (c->opened[k] && !c->closed[k]) {
      cs_buf_printf(out, "%scs_se%" PRIu32 "_%d:\n", v->prefix, c->id, (int)k);
      c->closed[k] = true;
    }
  }
  if (keep) {
    cs_buf_put(&v->funcs, (const char *)&c, sizeof(MirVarsFunc *));
  } else if (c != NULL) {
    cs_buf_free(&c->traps);
  }
  v->cur = NULL;
  return c;
}

size_t mir_vars_ntraps(const MirVarsFunc *f) {
  return f->traps.len / sizeof(int);
}

size_t mir_vars_chain(const MirVarsFunc *f, size_t j, int *chain) {
  int k;
  size_t n = 0;
  size_t i;
  memcpy(&k, f->traps.data + j * sizeof k, sizeof k);
  for (; k > 0; k = f->d.parent[k]) {
    chain[n++] = k;
  }
  for (i = 0; i < n / 2; i++) {
    int x = chain[i];
    chain[i] = chain[n - 1 - i];
    chain[n - 1 - i] = x;
  }
  return n;
}

void mir_debug_marker(CsBuf *out, char kind, int func, int n) {
  cs_buf_printf(out, ".Lcs_%c%d_%d", kind, func, n);
}

char mir_debug_find_marker(const char *line, size_t len, int *n) {
  size_t i;
  for (i = 0; i + 6 < len; i++) {
    if (memcmp(line + i, "Lcs_", 4) == 0 && (line[i + 4] == 'v' || line[i + 4] == 'o' || line[i + 4] == 'c' || line[i + 4] == 't') &&
        line[i + 5] >= '0' && line[i + 5] <= '9') {
      char kind = line[i + 4];
      size_t j = i + 5;
      int v = 0;
      while (j < len && line[j] >= '0' && line[j] <= '9') {
        j++;
      }
      if (j >= len || line[j] != '_') {
        continue;
      }
      for (j++; j < len && line[j] >= '0' && line[j] <= '9'; j++) {
        v = v * 10 + (line[j] - '0');
      }
      *n = v;
      return kind;
    }
  }
  return 0;
}
