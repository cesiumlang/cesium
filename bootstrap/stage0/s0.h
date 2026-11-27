/* Stage 0: the S0 frontend (cesiumlang/cesium#9).
 *
 * S0 source files -> tokens (s0_lex.c) -> syntax tree (s0_parse.c) ->
 * checked core SIR (s0_lower.c and the files of s0_lower.h, with s0_habit.c
 * for the messages that name the Cesium form of a habit from C). libcsir
 * verifies and lowers the SIR.
 *
 * Where the design leaves a question open, stage 0 rejects the program
 * instead of picking an answer, so that no S0 source can depend on a rule
 * that has not been decided. bootstrap/README.md lists those places.
 */
#ifndef CS_S0_H
#define CS_S0_H

#include "cs_common.h"
#include "sir_core.h"

/* ---- diagnostics ------------------------------------------------------- */

typedef struct S0Loc {
  const char *file;
  int line;
  int col;
} S0Loc;

typedef struct S0Diag {
  int errors;
} S0Diag;

/* Prints "file:line:col: error[code]: message" to stderr and counts it. The
 * code, such as "E0101", is the message's entry in bootstrap/diagnostics.md. */
void s0_error(S0Diag *d, S0Loc loc, const char *code, const char *fmt, ...);

/* ---- tokens ------------------------------------------------------------ */

typedef enum S0Tok {
  TK_EOF,
  TK_IDENT,
  TK_INT,     /* decimal integer literal */
  TK_CHAR,    /* 'x': one character or escape */
  TK_STRING,  /* "..." */
  TK_LIBNAME, /* '...' with more or fewer than one character (import('lib')) */
  /* keywords of S0 */
  TK_STRUCT,
  TK_EXTERN,
  TK_IMPORT,
  TK_CONST,
  TK_IF,
  TK_ELSE,
  TK_WHILE,
  TK_BREAK,
  TK_CONTINUE,
  TK_RETURN,
  TK_SIZEOF,
  TK_AS,
  TK_VOID,
  /* punctuation */
  TK_LPAREN,
  TK_RPAREN,
  TK_LBRACE,
  TK_RBRACE,
  TK_LBRACKET,
  TK_RBRACKET,
  TK_COMMA,
  TK_SEMI,
  TK_DOT,
  TK_COLON,
  TK_HASH,       /* #  */
  TK_TILDE,      /* ~  */
  TK_ASSIGN,     /* =  */
  TK_OWN,        /* := */
  TK_BORROW,     /* #= */
  TK_BORROW_MUT, /* ~= */
  TK_EQ,
  TK_NE,
  TK_LT,
  TK_LE,
  TK_GT,
  TK_GE,
  TK_PLUS,
  TK_MINUS,
  TK_STAR,
  TK_SLASH,
  TK_PERCENT,
  TK_AMP,
  TK_PIPE,
  TK_XOR,        /* >< */
  TK_SHL,
  TK_SHR,
  TK_ANDAND,
  TK_OROR,
  TK_BANG
} S0Tok;

typedef struct S0Token {
  S0Tok kind;
  S0Loc loc;
  const char *text; /* identifier; string or library-name bytes (unescaped) */
  size_t len;
  uint64_t ival;    /* TK_INT, TK_CHAR */
} S0Token;

typedef struct S0Lexer {
  CsArena *arena;
  S0Diag *diag;
  const char *file;
  const char *src;
  size_t len;
  size_t pos;
  int line;
  int col;
} S0Lexer;

void s0_lex_init(S0Lexer *lx, CsArena *arena, S0Diag *diag, const char *file, const char *src, size_t len);
/* The next token. After an error the lexer returns TK_EOF from then on. */
S0Token s0_lex_next(S0Lexer *lx);
/* A fixed description of a token kind, for messages. */
const char *s0_tok_name(S0Tok kind);

/* ---- syntax tree ------------------------------------------------------- */

typedef enum S0BaseType {
  BT_VOID,
  BT_U8,
  BT_I32,
  BT_U32,
  BT_I64,
  BT_U64,
  BT_UWORD,
  BT_STRUCT
} S0BaseType;

/* A type as written: sigils, then a base type. `#T` and `~T` are the same
 * to stage 0; so are owned (`:#T`) and borrowed forms. */
typedef struct S0TypeSyn {
  S0Loc loc;
  int depth;         /* number of # / ~ sigils */
  S0BaseType base;
  const char *name;  /* BT_STRUCT */
} S0TypeSyn;

typedef enum S0ExprKind {
  EX_INT,     /* integer or character literal: ival */
  EX_STRING,
  EX_NAME,
  EX_INDEX,   /* a[b] */
  EX_FIELD,   /* a.name */
  EX_CALL,    /* name(args) */
  EX_ADDROF,  /* #a or ~a */
  EX_NOT,     /* !a */
  EX_BINARY,  /* a op b */
  EX_AS,      /* a as type */
  EX_SIZEOF   /* sizeof(type) */
} S0ExprKind;

typedef struct S0Expr S0Expr;
struct S0Expr {
  S0ExprKind kind;
  S0Loc loc;
  S0Expr *a;
  S0Expr *b;
  S0Tok op;          /* EX_BINARY */
  uint64_t ival;
  const char *name;  /* EX_NAME, EX_FIELD, EX_CALL; EX_STRING bytes */
  size_t len;        /* EX_STRING */
  S0Expr **args;
  size_t nargs;
  S0TypeSyn type;    /* EX_AS, EX_SIZEOF */
};

typedef enum S0StmtKind {
  ST_DECL,    /* type name[count] [op init] */
  ST_ASSIGN,  /* place op value */
  ST_EXPR,
  ST_IF,
  ST_WHILE,
  ST_BREAK,
  ST_CONTINUE,
  ST_RETURN
} S0StmtKind;

typedef struct S0Stmt S0Stmt;
struct S0Stmt {
  S0StmtKind kind;
  S0Loc loc;
  S0TypeSyn type;     /* ST_DECL */
  const char *name;   /* ST_DECL */
  bool is_array;      /* ST_DECL */
  uint64_t count;     /* ST_DECL: array length */
  S0Tok op;           /* ST_DECL (TK_EOF when there is no initializer), ST_ASSIGN */
  S0Expr *a;          /* place, condition, expression, or returned value */
  S0Expr *b;          /* initializer or assigned value */
  S0Stmt *body;
  S0Stmt *els;
  S0Stmt *next;
};

typedef struct S0Param {
  S0TypeSyn type;
  const char *name;
  S0Loc loc;
} S0Param;

typedef enum S0DeclKind {
  DK_STRUCT,
  DK_CONST,
  DK_GLOBAL,
  DK_FUNC,
  DK_EXTERN_FUNC
} S0DeclKind;

typedef struct S0Decl S0Decl;
struct S0Decl {
  S0DeclKind kind;
  S0Loc loc;
  const char *name;
  S0TypeSyn type;     /* const and global: the type; functions: the return type */
  /* DK_STRUCT: fields are ST_DECL statements without initializers */
  S0Stmt *fields;
  /* DK_CONST, DK_GLOBAL */
  bool is_array;
  uint64_t count;
  S0Tok op;
  S0Expr *init;
  /* DK_FUNC, DK_EXTERN_FUNC */
  S0Param *params;
  size_t nparams;
  S0Stmt *body;
  const char *lib;    /* DK_EXTERN_FUNC: the import('lib') name */
  S0Decl *next;
};

/* Parses one file and appends its declarations to *head / *tail. */
void s0_parse_file(CsArena *arena, S0Diag *diag, const char *file, const char *src, size_t len, S0Decl **head,
                   S0Decl **tail);

/* ---- checking and lowering --------------------------------------------- */

/* Checks the whole unit and builds its core SIR. Returns NULL after
 * reporting errors. The caller frees the module. */
SirModule *s0_lower(CsArena *arena, S0Diag *diag, S0Decl *decls);

/* Habits from C and other languages (s0_habit.c): each reports the message
 * that names the Cesium form and returns true, or returns false when the
 * name is not one of them. For an unknown type and an unknown name. */
bool s0_habit_type(S0Diag *diag, S0Loc loc, const char *name);
bool s0_habit_name(S0Diag *diag, S0Loc loc, const char *name);

#endif
