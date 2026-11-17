/* Core MIR text: printer and parser (cesiumlang/cesium#8, "Core level
 * sketch"). The grammar, version 0:
 *
 *   module  := "cesium-mir" INT decl*
 *   decl    := "struct" NAME "{" [NAME ":" type {"," NAME ":" type}] "}"
 *            | "extern" "func" @NAME "(" [type {"," type}] ")" "->" type "callconv" "(" "c" ")"
 *            | ("const" | "global") @NAME ":" type ["=" init]
 *            | ["export"] "func" @NAME "(" [%NAME ":" type {"," ...}] ")" "->" type block
 *   init    := const | STRING | @NAME
 *   type    := NAME | "ptr" "<" type ">" | "[" INT "x" type "]"
 *   block   := "{" stmt* "}"
 *   stmt    := "local" %NAME ":" type ["=" expr]
 *            | "set" %NAME "=" expr
 *            | "store" expr "," expr
 *            | "eval" expr
 *            | "if" expr block ["else" block]
 *            | "while" expr block
 *            | "break" | "continue" | "return" [expr]
 *   expr    := const | %NAME | @NAME
 *            | "addr" "(" %NAME ")" | "field" "(" expr "," NAME ")"
 *            | "call" @NAME "(" [expr {"," expr}] ")"
 *            | "as" "(" type "," expr ")" | ("sizeof" | "alignof") "(" type ")"
 *            | OP "(" expr ["," expr] ")"
 *   const   := ["-"] INT ":" type
 *
 * ";" starts a comment that runs to the end of the line. Names may contain
 * letters, digits, "_" and ".". Strings escape a byte as "\HH" (two hex
 * digits); "\\" and "\"" are written that way.
 */
#include "mir.h"

/* ======================================================================== */
/* printer                                                                  */
/* ======================================================================== */

static void print_expr(CsBuf *b, const MirExpr *e);

static void print_const(CsBuf *b, const MirType *type, uint64_t v) {
  if (mir_type_is_signed(type)) {
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
  mir_type_print(b, type);
}

static void print_expr(CsBuf *b, const MirExpr *e) {
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
    mir_type_print(b, e->targ);
    cs_buf_puts(b, ", ");
    print_expr(b, e->a);
    cs_buf_putc(b, ')');
    break;
  case ME_SIZEOF:
  case ME_ALIGNOF:
    cs_buf_printf(b, "%s(", mir_op_name(e->op));
    mir_type_print(b, e->targ);
    cs_buf_putc(b, ')');
    break;
  default:
    cs_buf_printf(b, "%s(", mir_op_name(e->op));
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

static void print_block(CsBuf *b, const MirStmt *s, int depth);

static void print_stmt(CsBuf *b, const MirStmt *s, int depth) {
  print_indent(b, depth);
  switch (s->kind) {
  case MS_LOCAL:
    cs_buf_printf(b, "local %%%s: ", s->name);
    mir_type_print(b, s->type);
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
    print_block(b, s->body, depth + 1);
    print_indent(b, depth);
    cs_buf_putc(b, '}');
    if (s->els != NULL) {
      cs_buf_puts(b, " else {\n");
      print_block(b, s->els, depth + 1);
      print_indent(b, depth);
      cs_buf_putc(b, '}');
    }
    break;
  case MS_WHILE:
    cs_buf_puts(b, "while ");
    print_expr(b, s->a);
    cs_buf_puts(b, " {\n");
    print_block(b, s->body, depth + 1);
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

static void print_block(CsBuf *b, const MirStmt *s, int depth) {
  for (; s != NULL; s = s->next) {
    print_stmt(b, s, depth);
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

void mir_print(CsBuf *out, const MirModule *m) {
  const MirStruct *s;
  const MirGlobal *g;
  const MirFunc *f;
  size_t i;
  cs_buf_printf(out, "cesium-mir %d\n", MIR_VERSION);
  for (s = m->structs; s != NULL; s = s->next) {
    if (!s->defined) {
      continue;
    }
    cs_buf_printf(out, "\nstruct %s {", s->name);
    for (i = 0; i < s->nfields; i++) {
      cs_buf_printf(out, "%s%s: ", i == 0 ? " " : ", ", s->fields[i].name);
      mir_type_print(out, s->fields[i].type);
    }
    cs_buf_puts(out, " }\n");
  }
  if (m->globals != NULL) {
    cs_buf_putc(out, '\n');
  }
  for (g = m->globals; g != NULL; g = g->next) {
    cs_buf_printf(out, "%s @%s: ", g->is_const ? "const" : "global", g->name);
    mir_type_print(out, g->type);
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
        mir_type_print(out, f->params[i].type);
      }
      cs_buf_puts(out, ") -> ");
      mir_type_print(out, f->ret);
      cs_buf_puts(out, " callconv(c)\n");
      continue;
    }
    cs_buf_printf(out, "%sfunc @%s(", f->is_export ? "export " : "", f->name);
    for (i = 0; i < f->nparams; i++) {
      if (i != 0) {
        cs_buf_puts(out, ", ");
      }
      cs_buf_printf(out, "%%%s: ", f->params[i].name);
      mir_type_print(out, f->params[i].type);
    }
    cs_buf_puts(out, ") -> ");
    mir_type_print(out, f->ret);
    cs_buf_puts(out, " {\n");
    print_block(out, f->body, 1);
    cs_buf_puts(out, "}\n");
  }
}

/* ======================================================================== */
/* parser                                                                   */
/* ======================================================================== */

typedef enum Tok {
  T_EOF,
  T_NAME,   /* bare name: a type, operation or keyword */
  T_LOCAL,  /* %name */
  T_GLOBAL, /* @name */
  T_INT,
  T_STRING,
  T_PUNCT   /* one of ( ) { } [ ] < > , : = - and the two-character -> */
} Tok;

typedef struct Parser {
  MirModule *m;
  const char *src;
  size_t len;
  size_t pos;
  int line;
  /* current token */
  Tok tok;
  int tok_line;
  char punct;       /* T_PUNCT: the character; '>' with arrow set for "->" */
  bool arrow;
  uint64_t ival;
  bool int_overflow;
  const char *text; /* T_NAME, T_LOCAL, T_GLOBAL: the name; T_STRING: the bytes */
  size_t text_len;
} Parser;

static bool is_name_start(int c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool is_name_char(int c) {
  return is_name_start(c) || (c >= '0' && c <= '9') || c == '.';
}

static int hex_value(int c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  return -1;
}

static void perr(Parser *p, const char *fmt, ...) {
  va_list ap;
  char msg[256];
  va_start(ap, fmt);
  cs_vsnprintf(msg, sizeof msg, fmt, ap);
  va_end(ap);
  mir_error(p->m, p->tok_line, "%s", msg);
  /* stop consuming input so every loop ends */
  p->pos = p->len;
  p->tok = T_EOF;
}

static void next(Parser *p) {
  const char *s = p->src;
  if (p->m->failed) {
    p->tok = T_EOF;
    return;
  }
  for (;;) {
    while (p->pos < p->len && (s[p->pos] == ' ' || s[p->pos] == '\t' || s[p->pos] == '\r' || s[p->pos] == '\n')) {
      if (s[p->pos] == '\n') {
        p->line++;
      }
      p->pos++;
    }
    if (p->pos < p->len && s[p->pos] == ';') {
      while (p->pos < p->len && s[p->pos] != '\n') {
        p->pos++;
      }
      continue;
    }
    break;
  }
  p->tok_line = p->line;
  p->arrow = false;
  if (p->pos >= p->len) {
    p->tok = T_EOF;
    return;
  }
  {
    int c = (unsigned char)s[p->pos];
    if (c == '%' || c == '@') {
      size_t start = ++p->pos;
      while (p->pos < p->len && is_name_char((unsigned char)s[p->pos])) {
        p->pos++;
      }
      if (p->pos == start) {
        perr(p, "expected a name after '%c'", c);
        return;
      }
      p->tok = c == '%' ? T_LOCAL : T_GLOBAL;
      p->text = cs_arena_strn(&p->m->arena, s + start, p->pos - start);
      p->text_len = p->pos - start;
      return;
    }
    if (is_name_start(c)) {
      size_t start = p->pos;
      /* "cesium-mir" is the one name with a '-' in it */
      while (p->pos < p->len && (is_name_char((unsigned char)s[p->pos]) ||
                                 (s[p->pos] == '-' && p->pos - start == 6 && memcmp(s + start, "cesium", 6) == 0))) {
        p->pos++;
      }
      p->tok = T_NAME;
      p->text = cs_arena_strn(&p->m->arena, s + start, p->pos - start);
      p->text_len = p->pos - start;
      return;
    }
    if (c >= '0' && c <= '9') {
      uint64_t v = 0;
      p->int_overflow = false;
      while (p->pos < p->len && s[p->pos] >= '0' && s[p->pos] <= '9') {
        uint64_t d = (uint64_t)(s[p->pos] - '0');
        if (v > (UINT64_MAX - d) / 10) {
          p->int_overflow = true;
        }
        v = v * 10 + d;
        p->pos++;
      }
      p->tok = T_INT;
      p->ival = v;
      return;
    }
    if (c == '"') {
      CsBuf b = {NULL, 0, 0};
      p->pos++;
      for (;;) {
        if (p->pos >= p->len || s[p->pos] == '\n') {
          cs_buf_free(&b);
          perr(p, "unterminated string");
          return;
        }
        if (s[p->pos] == '"') {
          p->pos++;
          break;
        }
        if (s[p->pos] == '\\') {
          if (p->pos + 1 < p->len && (s[p->pos + 1] == '\\' || s[p->pos + 1] == '"')) {
            cs_buf_putc(&b, s[p->pos + 1]);
            p->pos += 2;
          } else if (p->pos + 2 < p->len && hex_value((unsigned char)s[p->pos + 1]) >= 0 &&
                     hex_value((unsigned char)s[p->pos + 2]) >= 0) {
            int v = hex_value((unsigned char)s[p->pos + 1]) * 16 + hex_value((unsigned char)s[p->pos + 2]);
            cs_buf_putc(&b, (char)(unsigned char)v);
            p->pos += 3;
          } else {
            cs_buf_free(&b);
            perr(p, "bad escape in string: expected \\HH, \\\\ or \\\"");
            return;
          }
        } else {
          cs_buf_putc(&b, s[p->pos]);
          p->pos++;
        }
      }
      p->tok = T_STRING;
      p->text = cs_arena_strn(&p->m->arena, b.data == NULL ? "" : b.data, b.len);
      p->text_len = b.len;
      cs_buf_free(&b);
      return;
    }
    if (c == '-' && p->pos + 1 < p->len && s[p->pos + 1] == '>') {
      p->pos += 2;
      p->tok = T_PUNCT;
      p->punct = '>';
      p->arrow = true;
      return;
    }
    if (strchr("(){}[]<>,:=-", c) != NULL) {
      p->pos++;
      p->tok = T_PUNCT;
      p->punct = (char)c;
      return;
    }
    perr(p, "unexpected character '%c'", c);
  }
}

static bool at_punct(const Parser *p, char c) {
  return p->tok == T_PUNCT && p->punct == c && !p->arrow;
}

static bool at_name(const Parser *p, const char *name) {
  return p->tok == T_NAME && strcmp(p->text, name) == 0;
}

static void expect_punct(Parser *p, char c) {
  if (!at_punct(p, c)) {
    perr(p, "expected '%c'", c);
    return;
  }
  next(p);
}

static void expect_name(Parser *p, const char *name) {
  if (!at_name(p, name)) {
    perr(p, "expected '%s'", name);
    return;
  }
  next(p);
}

/* Takes a token of the given kind and returns its text, or "" on an error. */
static const char *take(Parser *p, Tok kind, const char *what) {
  const char *text;
  if (p->tok != kind) {
    perr(p, "expected %s", what);
    return "";
  }
  text = p->text;
  next(p);
  return text;
}

static MirType *parse_type(Parser *p) {
  MirModule *m = p->m;
  if (at_punct(p, '[')) {
    uint64_t count;
    MirType *elem;
    next(p);
    if (p->tok != T_INT || p->int_overflow) {
      perr(p, "expected an array length");
      return mir_prim(m, MT_VOID);
    }
    count = p->ival;
    next(p);
    expect_name(p, "x");
    elem = parse_type(p);
    expect_punct(p, ']');
    return mir_array(m, elem, count);
  }
  if (p->tok == T_NAME) {
    const char *name = p->text;
    MirType *prim;
    next(p);
    if (strcmp(name, "ptr") == 0) {
      MirType *elem;
      expect_punct(p, '<');
      elem = parse_type(p);
      expect_punct(p, '>');
      return mir_ptr(m, elem);
    }
    prim = mir_prim_by_name(m, name);
    if (prim != NULL) {
      return prim;
    }
    return &mir_struct_ref(m, name)->type;
  }
  perr(p, "expected a type");
  return mir_prim(m, MT_VOID);
}

/* The largest magnitude an integer type holds, on each side of zero. */
static bool const_fits(const MirType *t, bool negative, uint64_t mag) {
  uint64_t max;
  switch (t->kind) {
  case MT_U8:
    max = UINT8_MAX;
    break;
  case MT_U16:
    max = UINT16_MAX;
    break;
  case MT_U32:
    max = UINT32_MAX;
    break;
  case MT_U64:
  case MT_UWORD:
    max = UINT64_MAX;
    break;
  case MT_I8:
    max = negative ? (uint64_t)INT8_MAX + 1 : (uint64_t)INT8_MAX;
    break;
  case MT_I16:
    max = negative ? (uint64_t)INT16_MAX + 1 : (uint64_t)INT16_MAX;
    break;
  case MT_I32:
    max = negative ? (uint64_t)INT32_MAX + 1 : (uint64_t)INT32_MAX;
    break;
  case MT_I64:
    max = negative ? (uint64_t)INT64_MAX + 1 : (uint64_t)INT64_MAX;
    break;
  default:
    return false;
  }
  if (negative && !mir_type_is_signed(t) && mag != 0) {
    return false;
  }
  return mag <= max;
}

static uint64_t truncate_to(const MirType *t, uint64_t v) {
  switch (t->kind) {
  case MT_U8:
  case MT_I8:
    return v & 0xff;
  case MT_U16:
  case MT_I16:
    return v & 0xffff;
  case MT_U32:
  case MT_I32:
    return v & 0xffffffff;
  default:
    return v;
  }
}

/* const := ["-"] INT ":" type; the current token is "-" or the INT */
static bool parse_const(Parser *p, MirType **type, uint64_t *value) {
  bool negative = false;
  uint64_t mag;
  if (at_punct(p, '-')) {
    negative = true;
    next(p);
  }
  if (p->tok != T_INT) {
    perr(p, "expected an integer");
    return false;
  }
  if (p->int_overflow) {
    perr(p, "integer constant is too large");
    return false;
  }
  mag = p->ival;
  next(p);
  expect_punct(p, ':');
  *type = parse_type(p);
  if (p->m->failed) {
    return false;
  }
  if (!mir_type_is_int(*type)) {
    perr(p, "a constant needs an integer type");
    return false;
  }
  if (!const_fits(*type, negative, mag)) {
    perr(p, "constant does not fit its type");
    return false;
  }
  *value = truncate_to(*type, negative ? (uint64_t)0 - mag : mag);
  return true;
}

static MirOp op_by_name(const char *name) {
  int op;
  for (op = ME_ADD; op < ME_OP_COUNT; op++) {
    if (strcmp(mir_op_name((MirOp)op), name) == 0) {
      return (MirOp)op;
    }
  }
  if (strcmp(name, "load") == 0) {
    return ME_LOAD;
  }
  if (strcmp(name, "index") == 0) {
    return ME_INDEX;
  }
  return ME_OP_COUNT;
}

static bool op_is_binary(MirOp op) {
  switch (op) {
  case ME_LOAD:
  case ME_NOT:
  case ME_NEG:
  case ME_LNOT:
    return false;
  default:
    return true;
  }
}

static MirExpr *parse_expr(Parser *p) {
  MirModule *m = p->m;
  int line = p->tok_line;
  if (p->tok == T_INT || at_punct(p, '-')) {
    MirType *type = NULL;
    uint64_t value = 0;
    if (!parse_const(p, &type, &value)) {
      return mir_const(m, mir_prim(m, MT_I64), 0, line);
    }
    return mir_const(m, type, value, line);
  }
  if (p->tok == T_LOCAL) {
    MirExpr *e = mir_local_ref(m, p->text, line);
    next(p);
    return e;
  }
  if (p->tok == T_GLOBAL) {
    MirExpr *e = mir_global_ref(m, p->text, line);
    next(p);
    return e;
  }
  if (p->tok == T_NAME) {
    const char *name = p->text;
    MirOp op;
    next(p);
    if (strcmp(name, "call") == 0) {
      const char *callee = take(p, T_GLOBAL, "a function name");
      MirExpr *args[64];
      MirExpr **copy;
      size_t n = 0;
      expect_punct(p, '(');
      while (!at_punct(p, ')') && !m->failed) {
        if (n == sizeof args / sizeof args[0]) {
          perr(p, "too many arguments");
          break;
        }
        args[n++] = parse_expr(p);
        if (!at_punct(p, ')')) {
          expect_punct(p, ',');
        }
      }
      expect_punct(p, ')');
      copy = (MirExpr **)cs_arena_new(&m->arena, cs_mul_size(n == 0 ? 1 : n, sizeof(MirExpr *)));
      if (n != 0) {
        memcpy(copy, args, n * sizeof(MirExpr *));
      }
      return mir_call(m, callee, copy, n, line);
    }
    expect_punct(p, '(');
    if (strcmp(name, "addr") == 0) {
      MirExpr *e = mir_expr(m, ME_ADDR, line);
      e->name = take(p, T_LOCAL, "a local name");
      expect_punct(p, ')');
      return e;
    }
    if (strcmp(name, "field") == 0) {
      MirExpr *ptr = parse_expr(p);
      const char *field;
      expect_punct(p, ',');
      field = take(p, T_NAME, "a field name");
      expect_punct(p, ')');
      return mir_field(m, ptr, field, line);
    }
    if (strcmp(name, "as") == 0) {
      MirType *to = parse_type(p);
      MirExpr *a;
      expect_punct(p, ',');
      a = parse_expr(p);
      expect_punct(p, ')');
      return mir_as(m, to, a, line);
    }
    if (strcmp(name, "sizeof") == 0 || strcmp(name, "alignof") == 0) {
      MirType *t = parse_type(p);
      expect_punct(p, ')');
      return mir_type_op(m, name[0] == 's' ? ME_SIZEOF : ME_ALIGNOF, t, line);
    }
    op = op_by_name(name);
    if (op == ME_OP_COUNT) {
      perr(p, "unknown operation '%s'", name);
      return mir_const(m, mir_prim(m, MT_I64), 0, line);
    }
    {
      MirExpr *a = parse_expr(p);
      MirExpr *b = NULL;
      if (op_is_binary(op)) {
        expect_punct(p, ',');
        b = parse_expr(p);
      }
      expect_punct(p, ')');
      return mir_binary(m, op, a, b, line);
    }
  }
  perr(p, "expected an expression");
  return mir_const(m, mir_prim(m, MT_I64), 0, line);
}

static MirStmt *parse_block(Parser *p);

static bool at_stmt_keyword(const Parser *p) {
  static const char *const kw[] = {"local", "set", "store", "eval", "if", "while", "break", "continue", "return"};
  size_t i;
  if (p->tok != T_NAME) {
    return false;
  }
  for (i = 0; i < sizeof kw / sizeof kw[0]; i++) {
    if (strcmp(p->text, kw[i]) == 0) {
      return true;
    }
  }
  return false;
}

static MirStmt *parse_stmt(Parser *p) {
  MirModule *m = p->m;
  int line = p->tok_line;
  MirStmt *s;
  if (p->tok != T_NAME) {
    perr(p, "expected a statement");
    return mir_stmt(m, MS_BREAK, line);
  }
  if (at_name(p, "local")) {
    next(p);
    s = mir_stmt(m, MS_LOCAL, line);
    s->name = take(p, T_LOCAL, "a local name");
    expect_punct(p, ':');
    s->type = parse_type(p);
    if (at_punct(p, '=')) {
      next(p);
      s->a = parse_expr(p);
    }
    return s;
  }
  if (at_name(p, "set")) {
    next(p);
    s = mir_stmt(m, MS_SET, line);
    s->name = take(p, T_LOCAL, "a local name");
    expect_punct(p, '=');
    s->a = parse_expr(p);
    return s;
  }
  if (at_name(p, "store")) {
    next(p);
    s = mir_stmt(m, MS_STORE, line);
    s->a = parse_expr(p);
    expect_punct(p, ',');
    s->b = parse_expr(p);
    return s;
  }
  if (at_name(p, "eval")) {
    next(p);
    s = mir_stmt(m, MS_EVAL, line);
    s->a = parse_expr(p);
    return s;
  }
  if (at_name(p, "if")) {
    next(p);
    s = mir_stmt(m, MS_IF, line);
    s->a = parse_expr(p);
    s->body = parse_block(p);
    if (at_name(p, "else")) {
      next(p);
      s->els = parse_block(p);
    }
    return s;
  }
  if (at_name(p, "while")) {
    next(p);
    s = mir_stmt(m, MS_WHILE, line);
    s->a = parse_expr(p);
    s->body = parse_block(p);
    return s;
  }
  if (at_name(p, "break")) {
    next(p);
    return mir_stmt(m, MS_BREAK, line);
  }
  if (at_name(p, "continue")) {
    next(p);
    return mir_stmt(m, MS_CONTINUE, line);
  }
  if (at_name(p, "return")) {
    next(p);
    s = mir_stmt(m, MS_RETURN, line);
    if (!at_punct(p, '}') && !at_stmt_keyword(p)) {
      s->a = parse_expr(p);
    }
    return s;
  }
  perr(p, "unknown statement '%s'", p->text);
  return mir_stmt(m, MS_BREAK, line);
}

static MirStmt *parse_block(Parser *p) {
  MirStmt *head = NULL;
  MirStmt *tail = NULL;
  expect_punct(p, '{');
  while (!at_punct(p, '}') && p->tok != T_EOF) {
    MirStmt *s = parse_stmt(p);
    if (tail != NULL) {
      tail->next = s;
    } else {
      head = s;
    }
    tail = s;
  }
  expect_punct(p, '}');
  return head;
}

#define MAX_PARAMS 64

static void parse_func(Parser *p, bool is_extern, bool is_export) {
  MirModule *m = p->m;
  int line = p->tok_line;
  const char *name;
  MirParam params[MAX_PARAMS];
  MirParam *copy;
  size_t n = 0;
  MirType *ret;
  MirFunc *f;
  expect_name(p, "func");
  name = take(p, T_GLOBAL, "a function name");
  expect_punct(p, '(');
  while (!at_punct(p, ')') && !m->failed) {
    if (n == MAX_PARAMS) {
      perr(p, "too many parameters");
      break;
    }
    if (is_extern) {
      params[n].name = NULL;
    } else {
      params[n].name = take(p, T_LOCAL, "a parameter name");
      expect_punct(p, ':');
    }
    params[n].type = parse_type(p);
    n++;
    if (!at_punct(p, ')')) {
      expect_punct(p, ',');
    }
  }
  expect_punct(p, ')');
  if (!(p->tok == T_PUNCT && p->arrow)) {
    perr(p, "expected '->'");
    return;
  }
  next(p);
  ret = parse_type(p);
  copy = (MirParam *)cs_arena_new(&m->arena, cs_mul_size(n == 0 ? 1 : n, sizeof(MirParam)));
  if (n != 0) {
    memcpy(copy, params, n * sizeof(MirParam));
  }
  f = mir_func_add(m, name, copy, n, ret, line);
  f->is_extern = is_extern;
  f->is_export = is_export;
  if (is_extern) {
    expect_name(p, "callconv");
    expect_punct(p, '(');
    expect_name(p, "c");
    expect_punct(p, ')');
  } else {
    f->body = parse_block(p);
  }
}

static void parse_struct(Parser *p) {
  MirModule *m = p->m;
  int line = p->tok_line;
  const char *name;
  MirField fields[256];
  MirField *copy;
  size_t n = 0;
  next(p);
  name = take(p, T_NAME, "a struct name");
  expect_punct(p, '{');
  while (!at_punct(p, '}') && !m->failed) {
    if (n == sizeof fields / sizeof fields[0]) {
      perr(p, "too many fields");
      break;
    }
    fields[n].name = take(p, T_NAME, "a field name");
    fields[n].offset = 0;
    expect_punct(p, ':');
    fields[n].type = parse_type(p);
    n++;
    if (!at_punct(p, '}')) {
      expect_punct(p, ',');
    }
  }
  expect_punct(p, '}');
  if (m->failed) {
    return;
  }
  copy = (MirField *)cs_arena_new(&m->arena, cs_mul_size(n == 0 ? 1 : n, sizeof(MirField)));
  if (n != 0) {
    memcpy(copy, fields, n * sizeof(MirField));
  }
  mir_struct_define(m, name, copy, n, line);
}

static void parse_global(Parser *p, bool is_const) {
  MirModule *m = p->m;
  int line = p->tok_line;
  const char *name;
  MirType *type;
  MirGlobal *g;
  next(p);
  name = take(p, T_GLOBAL, "a global name");
  expect_punct(p, ':');
  type = parse_type(p);
  if (m->failed) {
    return;
  }
  g = mir_global_add(m, name, type, is_const, line);
  if (!at_punct(p, '=')) {
    return;
  }
  next(p);
  if (p->tok == T_STRING) {
    g->init = MI_BYTES;
    g->bytes = p->text;
    g->nbytes = p->text_len;
    next(p);
  } else if (p->tok == T_GLOBAL) {
    g->init = MI_ADDR;
    g->addr_name = p->text;
    next(p);
  } else {
    MirType *ctype = NULL;
    uint64_t value = 0;
    if (parse_const(p, &ctype, &value)) {
      if (!mir_type_eq(ctype, type)) {
        perr(p, "initializer type differs from the type of @%s", name);
        return;
      }
      g->init = MI_INT;
      g->ival = value;
    }
  }
}

MirModule *mir_parse(const char *text, size_t len) {
  MirModule *m = mir_module_new();
  Parser p;
  memset(&p, 0, sizeof p);
  p.m = m;
  p.src = text;
  p.len = len;
  p.line = 1;
  next(&p);
  expect_name(&p, "cesium-mir");
  if (!m->failed) {
    if (p.tok != T_INT || p.ival != MIR_VERSION) {
      perr(&p, "expected MIR version %d", MIR_VERSION);
    } else {
      next(&p);
    }
  }
  while (p.tok != T_EOF && !m->failed) {
    if (at_name(&p, "struct")) {
      parse_struct(&p);
    } else if (at_name(&p, "extern")) {
      next(&p);
      parse_func(&p, true, false);
    } else if (at_name(&p, "export")) {
      next(&p);
      parse_func(&p, false, true);
    } else if (at_name(&p, "func")) {
      parse_func(&p, false, false);
    } else if (at_name(&p, "const")) {
      parse_global(&p, true);
    } else if (at_name(&p, "global")) {
      parse_global(&p, false);
    } else {
      perr(&p, "expected a declaration");
    }
  }
  return m;
}
