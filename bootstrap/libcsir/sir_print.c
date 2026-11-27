/* Core SIR text: the printer. The grammar is in sir_text.c, with the parser. */
#include "sir_core.h"

static void print_expr(CsBuf *b, const SirExpr *e);

static void print_const(CsBuf *b, const SirType *type, uint64_t v) {
  if (sir_type_is_signed(type)) {
    /* ival holds the bits truncated to the type; sign-extend to print */
    int bits = type->kind == MT_I8 ? 8 : type->kind == MT_I16 ? 16 : type->kind == MT_I32 ? 32 : 64;
    uint64_t sign = (uint64_t)1 << (bits - 1);
    if (v & sign) {
      uint64_t mag = bits == 64 ? (uint64_t)0 - v : (((uint64_t)1 << bits) - v);
      cs_buf_printf(b, "-%" PRIu64, mag);
    } else {
      cs_buf_printf(b, "%" PRIu64, v);
    }
  } else {
    cs_buf_printf(b, "%" PRIu64, v);
  }
  cs_buf_putc(b, ':');
  sir_type_print(b, type);
}

static void print_expr(CsBuf *b, const SirExpr *e) {
  size_t i;
  switch (e->op) {
  case ME_CONST:
    print_const(b, e->type, e->ival);
    break;
  case ME_LOCAL:
    cs_buf_printf(b, "%%%s", e->name);
    break;
  case ME_GLOBAL:
    cs_buf_printf(b, "@%s", e->name);
    break;
  case ME_ADDR:
    cs_buf_printf(b, "addr(%%%s)", e->name);
    break;
  case ME_FIELD:
    cs_buf_puts(b, "field(");
    print_expr(b, e->a);
    cs_buf_printf(b, ", %s)", e->name);
    break;
  case ME_CALL:
    cs_buf_printf(b, "call @%s(", e->name);
    for (i = 0; i < e->nargs; i++) {
      if (i != 0) {
        cs_buf_puts(b, ", ");
      }
      print_expr(b, e->args[i]);
    }
    cs_buf_putc(b, ')');
    break;
  case ME_AS:
    cs_buf_puts(b, "as(");
    sir_type_print(b, e->targ);
    cs_buf_puts(b, ", ");
    print_expr(b, e->a);
    cs_buf_putc(b, ')');
    break;
  case ME_SIZEOF:
  case ME_ALIGNOF:
    cs_buf_printf(b, "%s(", sir_op_name(e->op));
    sir_type_print(b, e->targ);
    cs_buf_putc(b, ')');
    break;
  default:
    cs_buf_printf(b, "%s(", sir_op_name(e->op));
    print_expr(b, e->a);
    if (e->b != NULL) {
      cs_buf_puts(b, ", ");
      print_expr(b, e->b);
    }
    cs_buf_putc(b, ')');
    break;
  }
}

static void print_indent(CsBuf *b, int depth) {
  int i;
  for (i = 0; i < depth; i++) {
    cs_buf_puts(b, "  ");
  }
}

static void print_bytes(CsBuf *b, const char *bytes, size_t n);

/* `loc ["file"] line:col` on a line of its own; the file is named when it is
 * not the one of the function's previous loc (*file, NULL for none yet). */
static void print_loc(CsBuf *b, const SirStmt *s, int depth, const char **file) {
  if (s->loc_line == 0) {
    return;
  }
  print_indent(b, depth);
  cs_buf_puts(b, "loc ");
  if (*file == NULL || strcmp(*file, s->loc_file) != 0) {
    print_bytes(b, s->loc_file, strlen(s->loc_file));
    cs_buf_putc(b, ' ');
    *file = s->loc_file;
  }
  cs_buf_printf(b, "%" PRIu32 ":%" PRIu32 "\n", s->loc_line, s->loc_col);
}

static void print_block(CsBuf *b, const SirStmt *s, int depth, const char **file);

static void print_stmt(CsBuf *b, const SirStmt *s, int depth, const char **file) {
  print_loc(b, s, depth, file);
  print_indent(b, depth);
  switch (s->kind) {
  case MS_LOCAL:
    cs_buf_printf(b, "local %%%s: ", s->name);
    sir_type_print(b, s->type);
    if (s->a != NULL) {
      cs_buf_puts(b, " = ");
      print_expr(b, s->a);
    }
    break;
  case MS_SET:
    cs_buf_printf(b, "set %%%s = ", s->name);
    print_expr(b, s->a);
    break;
  case MS_STORE:
    cs_buf_puts(b, "store ");
    print_expr(b, s->a);
    cs_buf_puts(b, ", ");
    print_expr(b, s->b);
    break;
  case MS_EVAL:
    cs_buf_puts(b, "eval ");
    print_expr(b, s->a);
    break;
  case MS_IF:
    cs_buf_puts(b, "if ");
    print_expr(b, s->a);
    cs_buf_puts(b, " {\n");
    print_block(b, s->body, depth + 1, file);
    print_indent(b, depth);
    cs_buf_putc(b, '}');
    if (s->els != NULL) {
      cs_buf_puts(b, " else {\n");
      print_block(b, s->els, depth + 1, file);
      print_indent(b, depth);
      cs_buf_putc(b, '}');
    }
    break;
  case MS_WHILE:
    cs_buf_puts(b, "while ");
    print_expr(b, s->a);
    cs_buf_puts(b, " {\n");
    print_block(b, s->body, depth + 1, file);
    print_indent(b, depth);
    cs_buf_putc(b, '}');
    break;
  case MS_BREAK:
    cs_buf_puts(b, "break");
    break;
  case MS_CONTINUE:
    cs_buf_puts(b, "continue");
    break;
  case MS_RETURN:
    cs_buf_puts(b, "return");
    if (s->a != NULL) {
      cs_buf_putc(b, ' ');
      print_expr(b, s->a);
    }
    break;
  }
  cs_buf_putc(b, '\n');
}

static void print_block(CsBuf *b, const SirStmt *s, int depth, const char **file) {
  for (; s != NULL; s = s->next) {
    print_stmt(b, s, depth, file);
  }
}

static void print_bytes(CsBuf *b, const char *bytes, size_t n) {
  size_t i;
  cs_buf_putc(b, '"');
  for (i = 0; i < n; i++) {
    unsigned char c = (unsigned char)bytes[i];
    if (c == '"' || c == '\\') {
      cs_buf_putc(b, '\\');
      cs_buf_putc(b, (char)c);
    } else if (c >= 0x20 && c < 0x7f) {
      cs_buf_putc(b, (char)c);
    } else {
      cs_buf_printf(b, "\\%02X", (unsigned)c);
    }
  }
  cs_buf_putc(b, '"');
}

void sir_print(CsBuf *out, const SirModule *m) {
  const SirStruct *s;
  const SirGlobal *g;
  const SirFunc *f;
  size_t i;
  cs_buf_printf(out, "cesium-sir %d\n", SIR_VERSION);
  for (s = m->structs; s != NULL; s = s->next) {
    if (!s->defined) {
      continue;
    }
    cs_buf_printf(out, "\nstruct %s {", s->name);
    for (i = 0; i < s->nfields; i++) {
      cs_buf_printf(out, "%s%s: ", i == 0 ? " " : ", ", s->fields[i].name);
      sir_type_print(out, s->fields[i].type);
    }
    cs_buf_puts(out, " }\n");
  }
  if (m->globals != NULL) {
    cs_buf_putc(out, '\n');
  }
  for (g = m->globals; g != NULL; g = g->next) {
    cs_buf_printf(out, "%s @%s: ", g->is_const ? "const" : "global", g->name);
    sir_type_print(out, g->type);
    switch (g->init) {
    case MI_ZERO:
      break;
    case MI_INT:
      cs_buf_puts(out, " = ");
      print_const(out, g->type, g->ival);
      break;
    case MI_BYTES:
      cs_buf_puts(out, " = ");
      print_bytes(out, g->bytes, g->nbytes);
      break;
    case MI_ADDR:
      cs_buf_printf(out, " = @%s", g->addr_name);
      break;
    }
    cs_buf_putc(out, '\n');
  }
  for (f = m->funcs; f != NULL; f = f->next) {
    cs_buf_putc(out, '\n');
    if (f->is_extern) {
      cs_buf_printf(out, "extern func @%s(", f->name);
      for (i = 0; i < f->nparams; i++) {
        if (i != 0) {
          cs_buf_puts(out, ", ");
        }
        sir_type_print(out, f->params[i].type);
      }
      cs_buf_puts(out, ") -> ");
      sir_type_print(out, f->ret);
      cs_buf_puts(out, " callconv(c)\n");
      continue;
    }
    cs_buf_printf(out, "%sfunc @%s(", f->is_export ? "export " : "", f->name);
    for (i = 0; i < f->nparams; i++) {
      if (i != 0) {
        cs_buf_puts(out, ", ");
      }
      cs_buf_printf(out, "%%%s: ", f->params[i].name);
      sir_type_print(out, f->params[i].type);
    }
    cs_buf_puts(out, ") -> ");
    sir_type_print(out, f->ret);
    cs_buf_puts(out, " {\n");
    {
      const char *file = NULL;
      print_block(out, f->body, 1, &file);
    }
    cs_buf_puts(out, "}\n");
  }
}
