/* The lowering of S0 to core SIR, shared by its files (cesiumlang/cesium#130):
 * s0_lower.c (values, types and names), s0_place.c, s0_expr.c, s0_stmt.c
 * and s0_decl.c, which has the entry point, s0_lower() of s0.h. The
 * rules are in s0_lower.c. */
#ifndef CS_S0_LOWER_H
#define CS_S0_LOWER_H

#include "s0.h"

typedef struct Const {
  const char *name;
  SirType *type;
  uint64_t value;
  struct Const *next;
} Const;

typedef struct Global {
  const char *name;
  SirType *type;
  struct Global *next;
} Global;

typedef struct Func {
  const char *name;
  S0Decl *decl;
  SirFunc *sir;
  struct Func *next;
} Func;

typedef struct StructName {
  const char *name;
  S0Decl *decl;
  struct StructName *next;
} StructName;

typedef struct Local {
  const char *name;
  SirType *type;
  struct Local *prev;
} Local;

typedef struct L {
  CsArena *arena;
  S0Diag *diag;
  SirModule *m;
  StructName *structs;
  Const *consts;
  Global *globals;
  Func *funcs;
  /* the function being lowered */
  SirFunc *fn;
  Local *locals;
  int loops;
  int nstrings;
  bool uses_alloc;
} L;

typedef struct Val {
  bool ok;
  SirExpr *e;
  SirType *t;
} Val;

/* Somewhere a value is stored: its address, and the type stored there. */
typedef struct Place {
  bool ok;
  SirExpr *addr;
  SirType *t;
  const char *local; /* set when the place is a whole local variable */
} Place;

/* How an expression is used. */
typedef struct Want {
  SirType *type;    /* the exact type wanted, or NULL */
  int ptr_depth;    /* type == NULL: > 0 wants any pointer of this depth (a cast's operand) */
  bool or_uword;    /* with ptr_depth: a uword is accepted too (integer -> pointer cast) */
  bool allow_void;  /* a call used as a statement */
} Want;

typedef struct Block {
  SirStmt *head;
  SirStmt *tail;
} Block;

/* s0_lower.c */
Val bad_val(void);
Val make_val(SirExpr *e, SirType *t);
Place bad_place(void);
int depth_of(const SirType *t);
const char *type_name(L *l, const SirType *t);
StructName *find_struct(L *l, const char *name);
SirType *resolve_type(L *l, const S0TypeSyn *syn);
bool literal_fits(const SirType *t, uint64_t v);
Local *find_local(L *l, const char *name);
Const *find_const(L *l, const char *name);
Global *find_global(L *l, const char *name);
Func *find_func(L *l, const char *name);
bool name_taken(L *l, const char *name, S0Loc loc, bool check_locals);
Want want_value(void);
Want want_type(SirType *t);

/* s0_place.c */
SirExpr *place_value(L *l, Place p, int line);
Place lower_place(L *l, S0Expr *e);
bool is_place_expr(const S0Expr *e);

/* s0_expr.c */
bool is_untyped(const S0Expr *e);
Val lower_string(L *l, S0Expr *e);
Val lower_call(L *l, S0Expr *e, Want w);
Val lower_expr(L *l, S0Expr *e, Want w);
Val lower_pointer_value(L *l, S0Expr *e, S0Tok op, SirType *to);

/* s0_stmt.c */
SirStmt *lower_block(L *l, S0Stmt *s);

#endif
