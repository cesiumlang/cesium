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

void mir_debug_marker(CsBuf *out, char kind, int func, int n) {
  cs_buf_printf(out, ".Lcs_%c%d_%d", kind, func, n);
}

char mir_debug_find_marker(const char *line, size_t len, int *n) {
  size_t i;
  for (i = 0; i + 6 < len; i++) {
    if (memcmp(line + i, "Lcs_", 4) == 0 && (line[i + 4] == 'v' || line[i + 4] == 'o' || line[i + 4] == 'c') &&
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
