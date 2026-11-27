/* Core SIR text: the parser (cesiumlang/cesium#8, "Core level sketch"); the
 * printer is sir_print.c. The grammar, version 0:
 *
 *   module  := "cesium-sir" INT decl*
 *   decl    := "struct" NAME "{" [NAME ":" type {"," NAME ":" type}] "}"
 *            | "extern" "func" @NAME "(" [type {"," type}] ")" "->" type "callconv" "(" "c" ")"
 *            | ("const" | "global") @NAME ":" type ["=" init]
 *            | ["export"] "func" @NAME "(" [%NAME ":" type {"," ...}] ")" "->" type block
 *   init    := const | STRING | @NAME
 *   type    := NAME | "ptr" "<" type ">" | "[" INT "x" type "]"
 *   block   := "{" ([loc] stmt)* "}"
 *   loc     := "loc" [STRING] INT ":" INT
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
 *
 * A loc is the source location of the statement after it: file, line and
 * column, each line and column from 1 to SIR_LOC_MAX - 1. The first loc of a
 * function names its file; a loc without one has the file of the previous
 * loc in the same function.
 */
#include "sir_core.h"

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
  SirModule *m;
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
  int depth;        /* of the type, expression or statement being parsed */
  const char *loc_file; /* the file of the function's previous loc, or NULL */
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

static void perr(Parser *p, const char *code, const char *fmt, ...) {
  va_list ap;
  char msg[256];
  va_start(ap, fmt);
  cs_vsnprintf(msg, sizeof msg, fmt, ap);
  va_end(ap);
  sir_error(p->m, p->tok_line, code, "%s", msg);
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
        perr(p, "E1001", "expected a name after '%c'", c);
        return;
      }
      p->tok = c == '%' ? T_LOCAL : T_GLOBAL;
      p->text = cs_arena_strn(&p->m->arena, s + start, p->pos - start);
      p->text_len = p->pos - start;
      return;
    }
    if (is_name_start(c)) {
      size_t start = p->pos;
      /* "cesium-sir" is the one name with a '-' in it */
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
          perr(p, "E1002", "unterminated string");
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
            perr(p, "E1003", "bad escape in string: expected \\HH, \\\\ or \\\"");
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
    perr(p, "E1004", "unexpected character '%c'", c);
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
    perr(p, "E1005", "expected '%c'", c);
    return;
  }
  next(p);
}

static void expect_name(Parser *p, const char *name) {
  if (!at_name(p, name)) {
    perr(p, "E1005", "expected '%s'", name);
    return;
  }
  next(p);
}

/* Takes a token of the given kind and returns its text, or "" on an error. */
static const char *take(Parser *p, Tok kind, const char *what) {
  const char *text;
  if (p->tok != kind) {
    perr(p, "E1005", "expected %s", what);
    return "";
  }
  text = p->text;
  next(p);
  return text;
}

/* The parser, the verifier, the printer and the lowering recurse as deep as
 * types, expressions and statements nest, so the nesting has a limit: without
 * one, a SIR file can overflow the stack. The limit is far above what the S0
 * front end produces from source within its own limit (stage0/s0_parse.c). */
#define MAX_NESTING 1024

static bool enter(Parser *p) {
  if (p->depth >= MAX_NESTING) {
    perr(p, "E1006", "nested too deeply (the limit is %d levels)", MAX_NESTING);
    return false;
  }
  p->depth++;
  return true;
}

static SirType *parse_type_at_depth(Parser *p);

static SirType *parse_type(Parser *p) {
  SirType *t;
  if (!enter(p)) {
    return sir_prim(p->m, MT_VOID);
  }
  t = parse_type_at_depth(p);
  p->depth--;
  return t;
}

static SirType *parse_type_at_depth(Parser *p) {
  SirModule *m = p->m;
  if (at_punct(p, '[')) {
    uint64_t count;
    SirType *elem;
    next(p);
    if (p->tok != T_INT || p->int_overflow) {
      perr(p, "E1005", "expected an array length");
      return sir_prim(m, MT_VOID);
    }
    count = p->ival;
    next(p);
    expect_name(p, "x");
    elem = parse_type(p);
    expect_punct(p, ']');
    return sir_array(m, elem, count);
  }
  if (p->tok == T_NAME) {
    const char *name = p->text;
    SirType *prim;
    next(p);
    if (strcmp(name, "ptr") == 0) {
      SirType *elem;
      expect_punct(p, '<');
      elem = parse_type(p);
      expect_punct(p, '>');
      return sir_ptr(m, elem);
    }
    prim = sir_prim_by_name(m, name);
    if (prim != NULL) {
      return prim;
    }
    return &sir_struct_ref(m, name)->type;
  }
  perr(p, "E1005", "expected a type");
  return sir_prim(m, MT_VOID);
}

/* The largest magnitude an integer type holds, on each side of zero. */
static bool const_fits(const SirType *t, bool negative, uint64_t mag) {
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
  if (negative && !sir_type_is_signed(t) && mag != 0) {
    return false;
  }
  return mag <= max;
}

static uint64_t truncate_to(const SirType *t, uint64_t v) {
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
static bool parse_const(Parser *p, SirType **type, uint64_t *value) {
  bool negative = false;
  uint64_t mag;
  if (at_punct(p, '-')) {
    negative = true;
    next(p);
  }
  if (p->tok != T_INT) {
    perr(p, "E1005", "expected an integer");
    return false;
  }
  if (p->int_overflow) {
    perr(p, "E1007", "integer constant is too large");
    return false;
  }
  mag = p->ival;
  next(p);
  expect_punct(p, ':');
  *type = parse_type(p);
  if (p->m->failed) {
    return false;
  }
  if (!sir_type_is_int(*type)) {
    perr(p, "E1106", "a constant needs an integer type");
    return false;
  }
  if (!const_fits(*type, negative, mag)) {
    perr(p, "E1008", "constant does not fit its type");
    return false;
  }
  *value = truncate_to(*type, negative ? (uint64_t)0 - mag : mag);
  return true;
}

static SirOp op_by_name(const char *name) {
  int op;
  for (op = ME_ADD; op < ME_OP_COUNT; op++) {
    if (strcmp(sir_op_name((SirOp)op), name) == 0) {
      return (SirOp)op;
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

static bool op_is_binary(SirOp op) {
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

static SirExpr *parse_expr_at_depth(Parser *p);

static SirExpr *parse_expr(Parser *p) {
  SirExpr *e;
  if (!enter(p)) {
    return sir_const(p->m, sir_prim(p->m, MT_I64), 0, p->tok_line);
  }
  e = parse_expr_at_depth(p);
  p->depth--;
  return e;
}

static SirExpr *parse_expr_at_depth(Parser *p) {
  SirModule *m = p->m;
  int line = p->tok_line;
  if (p->tok == T_INT || at_punct(p, '-')) {
    SirType *type = NULL;
    uint64_t value = 0;
    if (!parse_const(p, &type, &value)) {
      return sir_const(m, sir_prim(m, MT_I64), 0, line);
    }
    return sir_const(m, type, value, line);
  }
  if (p->tok == T_LOCAL) {
    SirExpr *e = sir_local_ref(m, p->text, line);
    next(p);
    return e;
  }
  if (p->tok == T_GLOBAL) {
    SirExpr *e = sir_global_ref(m, p->text, line);
    next(p);
    return e;
  }
  if (p->tok == T_NAME) {
    const char *name = p->text;
    SirOp op;
    next(p);
    if (strcmp(name, "call") == 0) {
      const char *callee = take(p, T_GLOBAL, "a function name");
      SirExpr *args[64];
      SirExpr **copy;
      size_t n = 0;
      expect_punct(p, '(');
      while (!at_punct(p, ')') && !m->failed) {
        if (n == sizeof args / sizeof args[0]) {
          perr(p, "E1009", "too many arguments");
          break;
        }
        args[n++] = parse_expr(p);
        if (!at_punct(p, ')')) {
          expect_punct(p, ',');
        }
      }
      expect_punct(p, ')');
      copy = (SirExpr **)cs_arena_new(&m->arena, cs_mul_size(n == 0 ? 1 : n, sizeof(SirExpr *)));
      if (n != 0) {
        memcpy(copy, args, n * sizeof(SirExpr *));
      }
      return sir_call(m, callee, copy, n, line);
    }
    expect_punct(p, '(');
    if (strcmp(name, "addr") == 0) {
      SirExpr *e = sir_expr(m, ME_ADDR, line);
      e->name = take(p, T_LOCAL, "a local name");
      expect_punct(p, ')');
      return e;
    }
    if (strcmp(name, "field") == 0) {
      SirExpr *ptr = parse_expr(p);
      const char *field;
      expect_punct(p, ',');
      field = take(p, T_NAME, "a field name");
      expect_punct(p, ')');
      return sir_field(m, ptr, field, line);
    }
    if (strcmp(name, "as") == 0) {
      SirType *to = parse_type(p);
      SirExpr *a;
      expect_punct(p, ',');
      a = parse_expr(p);
      expect_punct(p, ')');
      return sir_as(m, to, a, line);
    }
    if (strcmp(name, "sizeof") == 0 || strcmp(name, "alignof") == 0) {
      SirType *t = parse_type(p);
      expect_punct(p, ')');
      return sir_type_op(m, name[0] == 's' ? ME_SIZEOF : ME_ALIGNOF, t, line);
    }
    op = op_by_name(name);
    if (op == ME_OP_COUNT) {
      perr(p, "E1010", "unknown operation '%s'", name);
      return sir_const(m, sir_prim(m, MT_I64), 0, line);
    }
    {
      SirExpr *a = parse_expr(p);
      SirExpr *b = NULL;
      if (op_is_binary(op)) {
        expect_punct(p, ',');
        b = parse_expr(p);
      }
      expect_punct(p, ')');
      return sir_binary(m, op, a, b, line);
    }
  }
  perr(p, "E1005", "expected an expression");
  return sir_const(m, sir_prim(m, MT_I64), 0, line);
}

static SirStmt *parse_block(Parser *p);

static bool at_stmt_keyword(const Parser *p) {
  static const char *const kw[] = {"local", "set",      "store",  "eval", "if", "while",
                                   "break", "continue", "return", "loc"};
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

static SirStmt *parse_stmt_at_depth(Parser *p);

static SirStmt *parse_stmt(Parser *p) {
  SirStmt *s;
  if (!enter(p)) {
    return sir_stmt(p->m, MS_BREAK, p->tok_line);
  }
  s = parse_stmt_at_depth(p);
  p->depth--;
  return s;
}

static SirStmt *parse_stmt_at_depth(Parser *p) {
  SirModule *m = p->m;
  int line = p->tok_line;
  SirStmt *s;
  if (p->tok != T_NAME) {
    perr(p, "E1005", "expected a statement");
    return sir_stmt(m, MS_BREAK, line);
  }
  if (at_name(p, "local")) {
    next(p);
    s = sir_stmt(m, MS_LOCAL, line);
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
    s = sir_stmt(m, MS_SET, line);
    s->name = take(p, T_LOCAL, "a local name");
    expect_punct(p, '=');
    s->a = parse_expr(p);
    return s;
  }
  if (at_name(p, "store")) {
    next(p);
    s = sir_stmt(m, MS_STORE, line);
    s->a = parse_expr(p);
    expect_punct(p, ',');
    s->b = parse_expr(p);
    return s;
  }
  if (at_name(p, "eval")) {
    next(p);
    s = sir_stmt(m, MS_EVAL, line);
    s->a = parse_expr(p);
    return s;
  }
  if (at_name(p, "if")) {
    next(p);
    s = sir_stmt(m, MS_IF, line);
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
    s = sir_stmt(m, MS_WHILE, line);
    s->a = parse_expr(p);
    s->body = parse_block(p);
    return s;
  }
  if (at_name(p, "break")) {
    next(p);
    return sir_stmt(m, MS_BREAK, line);
  }
  if (at_name(p, "continue")) {
    next(p);
    return sir_stmt(m, MS_CONTINUE, line);
  }
  if (at_name(p, "return")) {
    next(p);
    s = sir_stmt(m, MS_RETURN, line);
    if (!at_punct(p, '}') && !at_stmt_keyword(p)) {
      s->a = parse_expr(p);
    }
    return s;
  }
  perr(p, "E1011", "unknown statement '%s'", p->text);
  return sir_stmt(m, MS_BREAK, line);
}

/* A line or column of a loc. */
static uint32_t parse_loc_number(Parser *p, const char *what) {
  uint32_t v;
  if (p->tok != T_INT) {
    perr(p, "E1005", "expected a %s number", what);
    return 0;
  }
  if (p->int_overflow || p->ival == 0 || p->ival >= SIR_LOC_MAX) {
    perr(p, "E1012", "a %s number in a loc is from 1 to %" PRIu32, what, SIR_LOC_MAX - 1);
    return 0;
  }
  v = (uint32_t)p->ival;
  next(p);
  return v;
}

/* An optional loc before a statement; gives it to s once s is parsed. */
static bool parse_loc(Parser *p, const char **file, uint32_t *line, uint32_t *col) {
  if (!at_name(p, "loc")) {
    return false;
  }
  next(p);
  if (p->tok == T_STRING) {
    if (p->text_len == 0) {
      perr(p, "E1013", "empty file name in a loc");
      return false;
    }
    if (memchr(p->text, '\0', p->text_len) != NULL) {
      perr(p, "E1014", "a file name in a loc has a zero byte");
      return false;
    }
    p->loc_file = p->text;
    next(p);
  } else if (p->loc_file == NULL) {
    perr(p, "E1015", "the first loc of a function names its file");
    return false;
  }
  *line = parse_loc_number(p, "line");
  expect_punct(p, ':');
  *col = parse_loc_number(p, "column");
  if (at_punct(p, '}') || p->tok == T_EOF) {
    perr(p, "E1016", "a loc stands before a statement");
  }
  *file = p->loc_file;
  return !p->m->failed;
}

static SirStmt *parse_block(Parser *p) {
  SirStmt *head = NULL;
  SirStmt *tail = NULL;
  expect_punct(p, '{');
  while (!at_punct(p, '}') && p->tok != T_EOF) {
    const char *file = NULL;
    uint32_t line = 0;
    uint32_t col = 0;
    bool has_loc = parse_loc(p, &file, &line, &col);
    SirStmt *s = parse_stmt(p);
    if (has_loc) {
      sir_stmt_loc(p->m, s, file, line, col);
    }
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
  SirModule *m = p->m;
  int line = p->tok_line;
  const char *name;
  SirParam params[MAX_PARAMS];
  SirParam *copy;
  size_t n = 0;
  SirType *ret;
  SirFunc *f;
  expect_name(p, "func");
  name = take(p, T_GLOBAL, "a function name");
  expect_punct(p, '(');
  while (!at_punct(p, ')') && !m->failed) {
    if (n == MAX_PARAMS) {
      perr(p, "E1017", "too many parameters");
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
    perr(p, "E1005", "expected '->'");
    return;
  }
  next(p);
  ret = parse_type(p);
  copy = (SirParam *)cs_arena_new(&m->arena, cs_mul_size(n == 0 ? 1 : n, sizeof(SirParam)));
  if (n != 0) {
    memcpy(copy, params, n * sizeof(SirParam));
  }
  f = sir_func_add(m, name, copy, n, ret, line);
  f->is_extern = is_extern;
  f->is_export = is_export;
  if (is_extern) {
    expect_name(p, "callconv");
    expect_punct(p, '(');
    expect_name(p, "c");
    expect_punct(p, ')');
  } else {
    p->loc_file = NULL;
    f->body = parse_block(p);
  }
}

static void parse_struct(Parser *p) {
  SirModule *m = p->m;
  int line = p->tok_line;
  const char *name;
  SirField fields[256];
  SirField *copy;
  size_t n = 0;
  next(p);
  name = take(p, T_NAME, "a struct name");
  expect_punct(p, '{');
  while (!at_punct(p, '}') && !m->failed) {
    if (n == sizeof fields / sizeof fields[0]) {
      perr(p, "E1018", "too many fields");
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
  copy = (SirField *)cs_arena_new(&m->arena, cs_mul_size(n == 0 ? 1 : n, sizeof(SirField)));
  if (n != 0) {
    memcpy(copy, fields, n * sizeof(SirField));
  }
  sir_struct_define(m, name, copy, n, line);
}

static void parse_global(Parser *p, bool is_const) {
  SirModule *m = p->m;
  int line = p->tok_line;
  const char *name;
  SirType *type;
  SirGlobal *g;
  next(p);
  name = take(p, T_GLOBAL, "a global name");
  expect_punct(p, ':');
  type = parse_type(p);
  if (m->failed) {
    return;
  }
  g = sir_global_add(m, name, type, is_const, line);
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
    SirType *ctype = NULL;
    uint64_t value = 0;
    if (parse_const(p, &ctype, &value)) {
      if (!sir_type_eq(ctype, type)) {
        perr(p, "E1019", "initializer type differs from the type of @%s", name);
        return;
      }
      g->init = MI_INT;
      g->ival = value;
    }
  }
}

SirModule *sir_parse(const char *text, size_t len) {
  SirModule *m = sir_module_new();
  Parser p;
  memset(&p, 0, sizeof p);
  p.m = m;
  p.src = text;
  p.len = len;
  p.line = 1;
  next(&p);
  expect_name(&p, "cesium-sir");
  if (!m->failed) {
    if (p.tok != T_INT || p.ival != SIR_VERSION) {
      perr(&p, "E1020", "expected SIR version %d", SIR_VERSION);
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
      perr(&p, "E1005", "expected a declaration");
    }
  }
  return m;
}
