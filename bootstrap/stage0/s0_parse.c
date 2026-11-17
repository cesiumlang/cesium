/* S0 parser: recursive descent for declarations and statements, precedence
 * climbing for expressions. The grammar is the S0 chapter of the draft
 * specification (cesiumlang.org, spec/s0-grammar), which transcribes
 * cesiumlang/cesium#9; binary precedence is the reference's table
 * (operators-expressions, levels 7 to 17).
 *
 * Not decided, and so chosen here (see bootstrap/README.md):
 *   - `!` is a prefix operator at the level of address-of, and `as` binds
 *     tighter than every binary operator (cesiumlang/cesium#15).
 */
#include "s0.h"

typedef struct Parser {
  CsArena *arena;
  S0Diag *diag;
  S0Lexer lx;
  S0Token tok;   /* current */
  S0Token peek;  /* next */
  bool failed;
} Parser;

static void advance(Parser *p) {
  p->tok = p->peek;
  p->peek = s0_lex_next(&p->lx);
}

/* Reports the first syntax error of the file and stops consuming input. */
static void fail(Parser *p, S0Loc loc, const char *fmt, ...) {
  va_list ap;
  char msg[256];
  if (p->failed) {
    return;
  }
  p->failed = true;
  va_start(ap, fmt);
  cs_vsnprintf(msg, sizeof msg, fmt, ap);
  va_end(ap);
  s0_error(p->diag, loc, "%s", msg);
  p->lx.pos = p->lx.len;
  p->tok.kind = TK_EOF;
  p->peek.kind = TK_EOF;
}

static bool at(const Parser *p, S0Tok kind) {
  return p->tok.kind == kind;
}

static bool accept(Parser *p, S0Tok kind) {
  if (at(p, kind)) {
    advance(p);
    return true;
  }
  return false;
}

static void expect(Parser *p, S0Tok kind) {
  if (!accept(p, kind)) {
    fail(p, p->tok.loc, "expected %s, found %s", s0_tok_name(kind), s0_tok_name(p->tok.kind));
  }
}

static const char *expect_ident(Parser *p) {
  const char *name = "";
  if (at(p, TK_IDENT)) {
    name = p->tok.text;
    advance(p);
  } else {
    fail(p, p->tok.loc, "expected a name, found %s", s0_tok_name(p->tok.kind));
  }
  return name;
}

/* ---- types ------------------------------------------------------------- */

static bool base_type_by_name(const char *name, S0BaseType *out) {
  static const struct {
    const char *name;
    S0BaseType base;
  } names[] = {
      {"u8", BT_U8}, {"i32", BT_I32}, {"u32", BT_U32}, {"i64", BT_I64}, {"u64", BT_U64}, {"uword", BT_UWORD},
  };
  size_t i;
  for (i = 0; i < sizeof names / sizeof names[0]; i++) {
    if (strcmp(names[i].name, name) == 0) {
      *out = names[i].base;
      return true;
    }
  }
  return false;
}

static bool is_base_type_name(const char *name) {
  S0BaseType b;
  return base_type_by_name(name, &b);
}

/* type = [":"] { "#" | "~" } base_type. The ":" (an owned parameter, return
 * value or field: cesiumlang/cesium#11, rules 22 to 25) is accepted where
 * allow_owned is set and must be followed by a sigil. */
static S0TypeSyn parse_type(Parser *p, bool allow_owned, bool allow_void) {
  S0TypeSyn t;
  memset(&t, 0, sizeof t);
  t.loc = p->tok.loc;
  if (at(p, TK_COLON)) {
    if (!allow_owned) {
      fail(p, p->tok.loc, "':' marks an owned parameter, return value or field; it cannot be used here");
      return t;
    }
    advance(p);
    if (!at(p, TK_HASH) && !at(p, TK_TILDE)) {
      fail(p, p->tok.loc, "':' must be followed by a pointer type");
      return t;
    }
  }
  while (at(p, TK_HASH) || at(p, TK_TILDE)) {
    t.depth++;
    advance(p);
  }
  if (at(p, TK_VOID)) {
    if (!allow_void || t.depth != 0) {
      fail(p, p->tok.loc, "'void' is only a return type in S0");
      return t;
    }
    advance(p);
    t.base = BT_VOID;
    return t;
  }
  if (!at(p, TK_IDENT)) {
    fail(p, p->tok.loc, "expected a type, found %s", s0_tok_name(p->tok.kind));
    return t;
  }
  if (!base_type_by_name(p->tok.text, &t.base)) {
    t.base = BT_STRUCT;
    t.name = p->tok.text;
  }
  advance(p);
  return t;
}

/* ---- expressions ------------------------------------------------------- */

static S0Expr *new_expr(Parser *p, S0ExprKind kind, S0Loc loc) {
  S0Expr *e = (S0Expr *)cs_arena_new(p->arena, sizeof(S0Expr));
  e->kind = kind;
  e->loc = loc;
  return e;
}

static S0Expr *parse_expr(Parser *p);

#define MAX_ARGS 64

static S0Expr *parse_primary(Parser *p) {
  S0Loc loc = p->tok.loc;
  S0Expr *e;
  if (at(p, TK_INT) || at(p, TK_CHAR)) {
    e = new_expr(p, EX_INT, loc);
    e->ival = p->tok.ival;
    advance(p);
    return e;
  }
  if (at(p, TK_STRING)) {
    e = new_expr(p, EX_STRING, loc);
    e->name = p->tok.text;
    e->len = p->tok.len;
    advance(p);
    return e;
  }
  if (at(p, TK_SIZEOF)) {
    advance(p);
    e = new_expr(p, EX_SIZEOF, loc);
    expect(p, TK_LPAREN);
    e->type = parse_type(p, false, false);
    expect(p, TK_RPAREN);
    return e;
  }
  if (at(p, TK_LPAREN)) {
    advance(p);
    e = parse_expr(p);
    expect(p, TK_RPAREN);
    return e;
  }
  if (at(p, TK_IDENT)) {
    const char *name = p->tok.text;
    advance(p);
    if (at(p, TK_LPAREN)) {
      S0Expr *args[MAX_ARGS];
      size_t n = 0;
      advance(p);
      while (!at(p, TK_RPAREN) && !p->failed) {
        if (n == MAX_ARGS) {
          fail(p, p->tok.loc, "too many arguments");
          break;
        }
        args[n++] = parse_expr(p);
        if (!at(p, TK_RPAREN)) {
          expect(p, TK_COMMA);
        }
      }
      expect(p, TK_RPAREN);
      e = new_expr(p, EX_CALL, loc);
      e->name = name;
      e->nargs = n;
      e->args = (S0Expr **)cs_arena_new(p->arena, cs_mul_size(n == 0 ? 1 : n, sizeof(S0Expr *)));
      if (n != 0) {
        memcpy(e->args, args, n * sizeof(S0Expr *));
      }
      return e;
    }
    e = new_expr(p, EX_NAME, loc);
    e->name = name;
    return e;
  }
  if (at(p, TK_MINUS)) {
    fail(p, loc, "S0 has no unary minus; write 0 - x");
  } else {
    fail(p, loc, "expected an expression, found %s", s0_tok_name(p->tok.kind));
  }
  return new_expr(p, EX_INT, loc);
}

static S0Expr *parse_postfix(Parser *p) {
  S0Expr *e = parse_primary(p);
  for (;;) {
    S0Loc loc = p->tok.loc;
    if (at(p, TK_LBRACKET)) {
      S0Expr *idx;
      S0Expr *n;
      advance(p);
      idx = parse_expr(p);
      expect(p, TK_RBRACKET);
      n = new_expr(p, EX_INDEX, loc);
      n->a = e;
      n->b = idx;
      e = n;
    } else if (at(p, TK_DOT)) {
      S0Expr *n;
      advance(p);
      n = new_expr(p, EX_FIELD, loc);
      n->a = e;
      n->name = expect_ident(p);
      e = n;
    } else if (at(p, TK_LPAREN)) {
      fail(p, loc, "only a function name can be called in S0");
      return e;
    } else {
      return e;
    }
  }
}

static S0Expr *parse_prefix(Parser *p) {
  S0Loc loc = p->tok.loc;
  if (at(p, TK_HASH) || at(p, TK_TILDE)) {
    S0Expr *e;
    advance(p);
    e = new_expr(p, EX_ADDROF, loc);
    e->a = parse_prefix(p);
    return e;
  }
  if (at(p, TK_BANG)) {
    S0Expr *e;
    advance(p);
    e = new_expr(p, EX_NOT, loc);
    e->a = parse_prefix(p);
    return e;
  }
  return parse_postfix(p);
}

static S0Expr *parse_cast(Parser *p) {
  S0Expr *e = parse_prefix(p);
  while (at(p, TK_AS)) {
    S0Expr *n = new_expr(p, EX_AS, p->tok.loc);
    advance(p);
    n->a = e;
    n->type = parse_type(p, false, false);
    e = n;
  }
  return e;
}

/* The reference's levels, lowest binding first. All group left to right. */
static int binary_level(S0Tok kind) {
  switch (kind) {
  case TK_OROR:
    return 1; /* level 17 */
  case TK_ANDAND:
    return 2; /* level 16 */
  case TK_PIPE:
    return 3; /* level 15 */
  case TK_XOR:
    return 4; /* level 14 */
  case TK_AMP:
    return 5; /* level 13 */
  case TK_EQ:
  case TK_NE:
    return 6; /* level 12 */
  case TK_LT:
  case TK_LE:
  case TK_GT:
  case TK_GE:
    return 7; /* level 11 */
  case TK_SHL:
  case TK_SHR:
    return 8; /* level 10 */
  case TK_PLUS:
  case TK_MINUS:
    return 9; /* level 9 */
  case TK_STAR:
  case TK_SLASH:
  case TK_PERCENT:
    return 10; /* level 7 */
  default:
    return 0;
  }
}

static S0Expr *parse_binary(Parser *p, int min_level) {
  S0Expr *lhs = parse_cast(p);
  for (;;) {
    int level = binary_level(p->tok.kind);
    S0Expr *n;
    if (level == 0 || level < min_level) {
      return lhs;
    }
    n = new_expr(p, EX_BINARY, p->tok.loc);
    n->op = p->tok.kind;
    advance(p);
    n->a = lhs;
    n->b = parse_binary(p, level + 1);
    lhs = n;
  }
}

static S0Expr *parse_expr(Parser *p) {
  return parse_binary(p, 1);
}

/* ---- statements -------------------------------------------------------- */

static S0Stmt *new_stmt(Parser *p, S0StmtKind kind, S0Loc loc) {
  S0Stmt *s = (S0Stmt *)cs_arena_new(p->arena, sizeof(S0Stmt));
  s->kind = kind;
  s->loc = loc;
  return s;
}

static S0Stmt *parse_block(Parser *p);

static bool is_assign_op(S0Tok kind) {
  return kind == TK_ASSIGN || kind == TK_OWN || kind == TK_BORROW || kind == TK_BORROW_MUT;
}

/* declarator [ initializer ] ";" after the type. Shared by locals, globals
 * and struct fields (which take no initializer). */
static void parse_declarator(Parser *p, S0Stmt *s, bool allow_init) {
  s->name = expect_ident(p);
  if (accept(p, TK_LBRACKET)) {
    if (!at(p, TK_INT)) {
      fail(p, p->tok.loc, "an array length is an integer literal in S0");
      return;
    }
    s->is_array = true;
    s->count = p->tok.ival;
    advance(p);
    expect(p, TK_RBRACKET);
  }
  s->op = TK_EOF;
  if (is_assign_op(p->tok.kind)) {
    if (!allow_init) {
      fail(p, p->tok.loc, "a struct field has no initializer");
      return;
    }
    s->op = p->tok.kind;
    advance(p);
    s->b = parse_expr(p);
  }
  expect(p, TK_SEMI);
}

/* A statement is a declaration when, after any sigils, it continues with a
 * primitive type name or with two names in a row. No expression has that
 * shape, so no symbol table is needed. */
static bool at_declaration(const Parser *p) {
  if (at(p, TK_HASH) || at(p, TK_TILDE)) {
    return true;
  }
  if (!at(p, TK_IDENT)) {
    return false;
  }
  return is_base_type_name(p->tok.text) || p->peek.kind == TK_IDENT;
}

static S0Stmt *parse_stmt(Parser *p) {
  S0Loc loc = p->tok.loc;
  S0Stmt *s;
  if (accept(p, TK_IF)) {
    s = new_stmt(p, ST_IF, loc);
    expect(p, TK_LPAREN);
    s->a = parse_expr(p);
    expect(p, TK_RPAREN);
    s->body = parse_block(p);
    if (accept(p, TK_ELSE)) {
      if (at(p, TK_IF)) {
        s->els = parse_stmt(p);
      } else {
        s->els = parse_block(p);
      }
    }
    return s;
  }
  if (accept(p, TK_WHILE)) {
    s = new_stmt(p, ST_WHILE, loc);
    expect(p, TK_LPAREN);
    s->a = parse_expr(p);
    expect(p, TK_RPAREN);
    s->body = parse_block(p);
    return s;
  }
  if (accept(p, TK_BREAK)) {
    expect(p, TK_SEMI);
    return new_stmt(p, ST_BREAK, loc);
  }
  if (accept(p, TK_CONTINUE)) {
    expect(p, TK_SEMI);
    return new_stmt(p, ST_CONTINUE, loc);
  }
  if (accept(p, TK_RETURN)) {
    s = new_stmt(p, ST_RETURN, loc);
    if (!at(p, TK_SEMI)) {
      s->a = parse_expr(p);
    }
    expect(p, TK_SEMI);
    return s;
  }
  if (at(p, TK_LBRACE)) {
    fail(p, loc, "a block is not a statement by itself in S0");
    return new_stmt(p, ST_BREAK, loc);
  }
  if (at_declaration(p)) {
    s = new_stmt(p, ST_DECL, loc);
    s->type = parse_type(p, false, false);
    parse_declarator(p, s, true);
    return s;
  }
  {
    S0Expr *e = parse_expr(p);
    if (is_assign_op(p->tok.kind)) {
      s = new_stmt(p, ST_ASSIGN, loc);
      s->a = e;
      s->op = p->tok.kind;
      advance(p);
      s->b = parse_expr(p);
    } else {
      s = new_stmt(p, ST_EXPR, loc);
      s->a = e;
    }
    expect(p, TK_SEMI);
    return s;
  }
}

static S0Stmt *parse_block(Parser *p) {
  S0Stmt *head = NULL;
  S0Stmt *tail = NULL;
  expect(p, TK_LBRACE);
  while (!at(p, TK_RBRACE) && !at(p, TK_EOF)) {
    S0Stmt *s = parse_stmt(p);
    if (tail != NULL) {
      tail->next = s;
    } else {
      head = s;
    }
    tail = s;
  }
  expect(p, TK_RBRACE);
  return head;
}

/* ---- declarations ------------------------------------------------------ */

static S0Decl *new_decl(Parser *p, S0DeclKind kind, S0Loc loc) {
  S0Decl *d = (S0Decl *)cs_arena_new(p->arena, sizeof(S0Decl));
  d->kind = kind;
  d->loc = loc;
  return d;
}

static void append(S0Decl **head, S0Decl **tail, S0Decl *d) {
  if (*tail != NULL) {
    (*tail)->next = d;
  } else {
    *head = d;
  }
  *tail = d;
}

#define MAX_PARAMS 64

static void parse_params(Parser *p, S0Decl *d) {
  S0Param params[MAX_PARAMS];
  size_t n = 0;
  expect(p, TK_LPAREN);
  while (!at(p, TK_RPAREN) && !p->failed) {
    if (n == MAX_PARAMS) {
      fail(p, p->tok.loc, "too many parameters");
      break;
    }
    params[n].loc = p->tok.loc;
    params[n].type = parse_type(p, true, false);
    params[n].name = expect_ident(p);
    n++;
    if (!at(p, TK_RPAREN)) {
      expect(p, TK_COMMA);
    }
  }
  expect(p, TK_RPAREN);
  d->nparams = n;
  d->params = (S0Param *)cs_arena_new(p->arena, cs_mul_size(n == 0 ? 1 : n, sizeof(S0Param)));
  if (n != 0) {
    memcpy(d->params, params, n * sizeof(S0Param));
  }
}

static void parse_struct(Parser *p, S0Decl **head, S0Decl **tail) {
  S0Decl *d = new_decl(p, DK_STRUCT, p->tok.loc);
  S0Stmt *ftail = NULL;
  advance(p);
  d->name = expect_ident(p);
  expect(p, TK_LBRACE);
  while (!at(p, TK_RBRACE) && !at(p, TK_EOF)) {
    S0Stmt *f = new_stmt(p, ST_DECL, p->tok.loc);
    f->type = parse_type(p, true, false);
    parse_declarator(p, f, false);
    if (ftail != NULL) {
      ftail->next = f;
    } else {
      d->fields = f;
    }
    ftail = f;
  }
  expect(p, TK_RBRACE);
  append(head, tail, d);
}

/* extern name = import('lib') { ret = name(params); ... } */
static void parse_extern(Parser *p, S0Decl **head, S0Decl **tail) {
  const char *lib = "";
  advance(p);
  (void)expect_ident(p);
  expect(p, TK_ASSIGN);
  expect(p, TK_IMPORT);
  expect(p, TK_LPAREN);
  if (at(p, TK_LIBNAME) || at(p, TK_CHAR) || at(p, TK_STRING)) {
    lib = p->tok.text;
    advance(p);
  } else {
    fail(p, p->tok.loc, "expected a library name such as 'c'");
  }
  expect(p, TK_RPAREN);
  expect(p, TK_LBRACE);
  while (!at(p, TK_RBRACE) && !at(p, TK_EOF)) {
    S0Decl *d = new_decl(p, DK_EXTERN_FUNC, p->tok.loc);
    d->lib = lib;
    d->type = parse_type(p, true, true);
    expect(p, TK_ASSIGN);
    d->name = expect_ident(p);
    parse_params(p, d);
    expect(p, TK_SEMI);
    append(head, tail, d);
  }
  expect(p, TK_RBRACE);
}

void s0_parse_file(CsArena *arena, S0Diag *diag, const char *file, const char *src, size_t len, S0Decl **head,
                   S0Decl **tail) {
  Parser p;
  memset(&p, 0, sizeof p);
  p.arena = arena;
  p.diag = diag;
  s0_lex_init(&p.lx, arena, diag, file, src, len);
  p.tok = s0_lex_next(&p.lx);
  p.peek = s0_lex_next(&p.lx);
  while (!at(&p, TK_EOF)) {
    S0Loc loc = p.tok.loc;
    if (at(&p, TK_STRUCT)) {
      parse_struct(&p, head, tail);
    } else if (at(&p, TK_EXTERN)) {
      parse_extern(&p, head, tail);
    } else if (at(&p, TK_CONST)) {
      S0Decl *d = new_decl(&p, DK_CONST, loc);
      advance(&p);
      d->type = parse_type(&p, false, false);
      d->name = expect_ident(&p);
      expect(&p, TK_ASSIGN);
      d->op = TK_ASSIGN;
      d->init = parse_expr(&p);
      expect(&p, TK_SEMI);
      append(head, tail, d);
    } else {
      /* a function `ret = name(params) { ... }` or a global `type name ...;`:
       * after the type, "=" means a function */
      S0TypeSyn type = parse_type(&p, true, true);
      if (accept(&p, TK_ASSIGN)) {
        S0Decl *d = new_decl(&p, DK_FUNC, loc);
        d->type = type;
        d->name = expect_ident(&p);
        parse_params(&p, d);
        d->body = parse_block(&p);
        append(head, tail, d);
      } else {
        S0Decl *d = new_decl(&p, DK_GLOBAL, loc);
        S0Stmt tmp;
        memset(&tmp, 0, sizeof tmp);
        if (type.base == BT_VOID) {
          fail(&p, loc, "'void' is only a return type in S0");
        }
        parse_declarator(&p, &tmp, true);
        d->type = type;
        d->name = tmp.name;
        d->is_array = tmp.is_array;
        d->count = tmp.count;
        d->op = tmp.op;
        d->init = tmp.b;
        append(head, tail, d);
      }
    }
    if (p.failed) {
      break;
    }
  }
}
