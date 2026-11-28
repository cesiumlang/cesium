/* S0 lexer. */
#include "s0.h"

void s0_error(S0Diag *d, S0Loc loc, const char *code, const char *fmt, ...) {
  va_list ap;
  char msg[512];
  va_start(ap, fmt);
  cs_vsnprintf(msg, sizeof msg, fmt, ap);
  va_end(ap);
  fprintf(stderr, "%s:%d:%d: error[%s]: %s\n", loc.file, loc.line, loc.col, code, msg);
  d->errors++;
}

void s0_lex_init(S0Lexer *lx, CsArena *arena, S0Diag *diag, const char *file, const char *src, size_t len) {
  lx->arena = arena;
  lx->diag = diag;
  lx->file = file;
  lx->src = src;
  lx->len = len;
  lx->pos = 0;
  lx->line = 1;
  lx->col = 1;
}

typedef struct Word {
  const char *text;
  S0Tok kind;
} Word;

static const Word s0_keywords[] = {
    {"struct", TK_STRUCT},     {"extern", TK_EXTERN}, {"import", TK_IMPORT}, {"const", TK_CONST},
    {"if", TK_IF},             {"else", TK_ELSE},     {"while", TK_WHILE},   {"break", TK_BREAK},
    {"continue", TK_CONTINUE}, {"return", TK_RETURN}, {"sizeof", TK_SIZEOF}, {"as", TK_AS},
    {"void", TK_VOID},
};

/* Keywords of full Cesium that S0 does not use (the keyword list of the
 * language reference, lexical-structure page). An S0 program must stay a
 * valid Cesium program, so these cannot be identifiers either. */
static const char *const cesium_reserved[] = {
    "file",     "str",      "list",   "dict",        "slice",     "path",     "alias",    "catch",
    "comptime", "generic",  "typeof", "asm",         "enum",      "error",    "null",     "undefined",
    "union",    "for",      "do",     "with",        "defer",     "switch",   "case",     "fallthrough",
    "throw",    "operator", "context", "property",   "private",   "secret",   "static",   "destruct",
    "volatile", "atomic",   "register", "simd",      "export",    "trait",    "impl",     "type",
    "this",     "super",    "std",    "namespace",
};

static bool is_ident_start(int c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool is_digit(int c) {
  return c >= '0' && c <= '9';
}

static int peek(const S0Lexer *lx, size_t ahead) {
  return lx->pos + ahead < lx->len ? (unsigned char)lx->src[lx->pos + ahead] : -1;
}

static void advance(S0Lexer *lx) {
  if (lx->pos < lx->len) {
    if (lx->src[lx->pos] == '\n') {
      lx->line++;
      lx->col = 1;
    } else {
      lx->col++;
    }
    lx->pos++;
  }
}

static S0Token fail(S0Lexer *lx, S0Loc loc, const char *code, const char *msg) {
  S0Token t;
  memset(&t, 0, sizeof t);
  s0_error(lx->diag, loc, code, "%s", msg);
  lx->pos = lx->len; /* every later call returns TK_EOF */
  t.kind = TK_EOF;
  t.loc = loc;
  return t;
}

/* Reads one character of a literal, handling an escape. The escape set of
 * Cesium is not decided (cesiumlang/cesium#39); these are the ones stage 0
 * accepts. Returns -1 on an error. */
static int read_char(S0Lexer *lx) {
  int c = peek(lx, 0);
  if (c != '\\') {
    advance(lx);
    return c;
  }
  advance(lx);
  c = peek(lx, 0);
  advance(lx);
  switch (c) {
  case 'n':
    return '\n';
  case 't':
    return '\t';
  case 'r':
    return '\r';
  case '0':
    return 0;
  case '\\':
    return '\\';
  case '\'':
    return '\'';
  case '"':
    return '"';
  default:
    return -1;
  }
}

static bool skip_space_and_comments(S0Lexer *lx, S0Loc *err) {
  for (;;) {
    int c = peek(lx, 0);
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
      advance(lx);
    } else if (c == '/' && peek(lx, 1) == '/') {
      while (peek(lx, 0) != -1 && peek(lx, 0) != '\n') {
        advance(lx);
      }
    } else if (c == '/' && peek(lx, 1) == '*') {
      err->file = lx->file;
      err->line = lx->line;
      err->col = lx->col;
      advance(lx);
      advance(lx);
      for (;;) {
        if (peek(lx, 0) == -1) {
          return false;
        }
        if (peek(lx, 0) == '*' && peek(lx, 1) == '/') {
          advance(lx);
          advance(lx);
          break;
        }
        advance(lx);
      }
    } else {
      return true;
    }
  }
}

S0Token s0_lex_next(S0Lexer *lx) {
  S0Token t;
  S0Loc loc;
  int c;
  size_t i;
  memset(&t, 0, sizeof t);
  loc.file = lx->file;
  loc.line = lx->line;
  loc.col = lx->col;
  if (!skip_space_and_comments(lx, &loc)) {
    return fail(lx, loc, "E0101", "unterminated comment");
  }
  loc.file = lx->file;
  loc.line = lx->line;
  loc.col = lx->col;
  t.loc = loc;
  c = peek(lx, 0);
  if (c == -1) {
    t.kind = TK_EOF;
    return t;
  }

  if (is_ident_start(c)) {
    size_t start = lx->pos;
    while (is_ident_start(peek(lx, 0)) || is_digit(peek(lx, 0))) {
      advance(lx);
    }
    t.text = cs_arena_strn(lx->arena, lx->src + start, lx->pos - start);
    t.len = lx->pos - start;
    t.kind = TK_IDENT;
    for (i = 0; i < sizeof s0_keywords / sizeof s0_keywords[0]; i++) {
      if (strcmp(t.text, s0_keywords[i].text) == 0) {
        t.kind = s0_keywords[i].kind;
        return t;
      }
    }
    for (i = 0; i < sizeof cesium_reserved / sizeof cesium_reserved[0]; i++) {
      if (strcmp(t.text, cesium_reserved[i]) == 0) {
        s0_error(lx->diag, loc, "E0102", "'%s' is a Cesium keyword that S0 does not have", t.text);
        lx->pos = lx->len;
        t.kind = TK_EOF;
        return t;
      }
    }
    return t;
  }

  if (is_digit(c)) {
    uint64_t v = 0;
    bool overflow = false;
    while (is_digit(peek(lx, 0))) {
      uint64_t d = (uint64_t)(peek(lx, 0) - '0');
      if (v > (UINT64_MAX - d) / 10) {
        overflow = true;
      }
      v = v * 10 + d;
      advance(lx);
    }
    if (is_ident_start(peek(lx, 0))) {
      return fail(lx, loc, "E0103", "S0 has decimal integer literals only");
    }
    if (overflow) {
      return fail(lx, loc, "E0104", "integer literal is too large");
    }
    t.kind = TK_INT;
    t.ival = v;
    return t;
  }

  if (c == '"' || c == '\'') {
    CsBuf b = {NULL, 0, 0};
    int quote = c;
    size_t nchars = 0;
    advance(lx);
    for (;;) {
      int ch = peek(lx, 0);
      if (ch == -1 || ch == '\n') {
        cs_buf_free(&b);
        return fail(lx, loc, "E0105", "unterminated literal");
      }
      if (ch == quote) {
        advance(lx);
        break;
      }
      ch = read_char(lx);
      if (ch < 0) {
        cs_buf_free(&b);
        return fail(lx, loc, "E0106", "unknown escape: stage 0 accepts \\n \\t \\r \\0 \\\\ \\' \\\"");
      }
      /* lex.lit.char.ascii */
      if (quote == '\'' && ch >= 0x80) {
        cs_buf_free(&b);
        return fail(lx, loc, "E0108", "a character literal holds one ASCII character; write other text as a string");
      }
      cs_buf_putc(&b, (char)(unsigned char)ch);
      nchars++;
    }
    t.text = cs_arena_strn(lx->arena, b.data == NULL ? "" : b.data, b.len);
    t.len = b.len;
    cs_buf_free(&b);
    if (quote == '"') {
      t.kind = TK_STRING;
    } else if (nchars == 1) {
      t.kind = TK_CHAR;
      t.ival = (unsigned char)t.text[0];
    } else {
      t.kind = TK_LIBNAME;
    }
    return t;
  }

  advance(lx);
  switch (c) {
  case '(':
    t.kind = TK_LPAREN;
    return t;
  case ')':
    t.kind = TK_RPAREN;
    return t;
  case '{':
    t.kind = TK_LBRACE;
    return t;
  case '}':
    t.kind = TK_RBRACE;
    return t;
  case '[':
    t.kind = TK_LBRACKET;
    return t;
  case ']':
    t.kind = TK_RBRACKET;
    return t;
  case ',':
    t.kind = TK_COMMA;
    return t;
  case ';':
    t.kind = TK_SEMI;
    return t;
  case '.':
    t.kind = TK_DOT;
    return t;
  case '+':
    t.kind = TK_PLUS;
    return t;
  case '-':
    t.kind = TK_MINUS;
    return t;
  case '*':
    t.kind = TK_STAR;
    return t;
  case '/':
    t.kind = TK_SLASH;
    return t;
  case '%':
    t.kind = TK_PERCENT;
    return t;
  case ':':
    if (peek(lx, 0) == '=') {
      advance(lx);
      t.kind = TK_OWN;
    } else {
      t.kind = TK_COLON;
    }
    return t;
  case '#':
    /* "#=" is the borrow operator, except in "#==": no S0 expression starts
     * with "=", so "#=" followed by "=" can only be a mistake left to the
     * parser */
    if (peek(lx, 0) == '=' && peek(lx, 1) != '=') {
      advance(lx);
      t.kind = TK_BORROW;
    } else {
      t.kind = TK_HASH;
    }
    return t;
  case '~':
    if (peek(lx, 0) == '=' && peek(lx, 1) != '=') {
      advance(lx);
      t.kind = TK_BORROW_MUT;
    } else {
      t.kind = TK_TILDE;
    }
    return t;
  case '=':
    if (peek(lx, 0) == '=') {
      advance(lx);
      t.kind = TK_EQ;
    } else {
      t.kind = TK_ASSIGN;
    }
    return t;
  case '!':
    if (peek(lx, 0) == '=') {
      advance(lx);
      t.kind = TK_NE;
    } else {
      t.kind = TK_BANG;
    }
    return t;
  case '<':
    if (peek(lx, 0) == '=') {
      advance(lx);
      t.kind = TK_LE;
    } else if (peek(lx, 0) == '<') {
      advance(lx);
      t.kind = TK_SHL;
    } else {
      t.kind = TK_LT;
    }
    return t;
  case '>':
    if (peek(lx, 0) == '=') {
      advance(lx);
      t.kind = TK_GE;
    } else if (peek(lx, 0) == '>') {
      advance(lx);
      t.kind = TK_SHR;
    } else if (peek(lx, 0) == '<') {
      advance(lx);
      t.kind = TK_XOR;
    } else {
      t.kind = TK_GT;
    }
    return t;
  case '&':
    if (peek(lx, 0) == '&') {
      advance(lx);
      t.kind = TK_ANDAND;
    } else {
      t.kind = TK_AMP;
    }
    return t;
  case '|':
    if (peek(lx, 0) == '|') {
      advance(lx);
      t.kind = TK_OROR;
    } else {
      t.kind = TK_PIPE;
    }
    return t;
  default:
    break;
  }
  return fail(lx, loc, "E0107", "unexpected character");
}

const char *s0_tok_name(S0Tok kind) {
  switch (kind) {
  case TK_EOF:
    return "end of file";
  case TK_IDENT:
    return "a name";
  case TK_INT:
    return "an integer";
  case TK_CHAR:
    return "a character";
  case TK_STRING:
    return "a string";
  case TK_LIBNAME:
    return "a library name";
  case TK_STRUCT:
    return "'struct'";
  case TK_EXTERN:
    return "'extern'";
  case TK_IMPORT:
    return "'import'";
  case TK_CONST:
    return "'const'";
  case TK_IF:
    return "'if'";
  case TK_ELSE:
    return "'else'";
  case TK_WHILE:
    return "'while'";
  case TK_BREAK:
    return "'break'";
  case TK_CONTINUE:
    return "'continue'";
  case TK_RETURN:
    return "'return'";
  case TK_SIZEOF:
    return "'sizeof'";
  case TK_AS:
    return "'as'";
  case TK_VOID:
    return "'void'";
  case TK_LPAREN:
    return "'('";
  case TK_RPAREN:
    return "')'";
  case TK_LBRACE:
    return "'{'";
  case TK_RBRACE:
    return "'}'";
  case TK_LBRACKET:
    return "'['";
  case TK_RBRACKET:
    return "']'";
  case TK_COMMA:
    return "','";
  case TK_SEMI:
    return "';'";
  case TK_DOT:
    return "'.'";
  case TK_COLON:
    return "':'";
  case TK_HASH:
    return "'#'";
  case TK_TILDE:
    return "'~'";
  case TK_ASSIGN:
    return "'='";
  case TK_OWN:
    return "':='";
  case TK_BORROW:
    return "'#='";
  case TK_BORROW_MUT:
    return "'~='";
  case TK_EQ:
    return "'=='";
  case TK_NE:
    return "'!='";
  case TK_LT:
    return "'<'";
  case TK_LE:
    return "'<='";
  case TK_GT:
    return "'>'";
  case TK_GE:
    return "'>='";
  case TK_PLUS:
    return "'+'";
  case TK_MINUS:
    return "'-'";
  case TK_STAR:
    return "'*'";
  case TK_SLASH:
    return "'/'";
  case TK_PERCENT:
    return "'%'";
  case TK_AMP:
    return "'&'";
  case TK_PIPE:
    return "'|'";
  case TK_XOR:
    return "'><'";
  case TK_SHL:
    return "'<<'";
  case TK_SHR:
    return "'>>'";
  case TK_ANDAND:
    return "'&&'";
  case TK_OROR:
    return "'||'";
  case TK_BANG:
    return "'!'";
  }
  return "a token";
}
