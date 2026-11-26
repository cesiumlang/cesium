/* Core MIR -> QBE IL (cesiumlang/cesium#8, "Lowering to QBE IL").
 *
 *   - locals are stack slots (QBE promotes them to registers);
 *   - structured control flow becomes labels and jumps;
 *   - 8- and 16-bit integers are computed in 32-bit temporaries and kept
 *     extended by their signedness after every operation;
 *   - index, field, sizeof and alignof are resolved with the target's layout;
 *   - comparisons and logical operations give 0 or 1;
 *   - source locations become dbgfile and dbgloc (line tables only).
 *
 * Defined semantics (no undefined behavior in core MIR):
 *   - add, sub, mul, neg wrap;
 *   - shift amounts are reduced modulo the operand's width;
 *   - division or remainder by zero, and signed MIN / -1, trap (hlt);
 *   - a local declared without an initializer starts as all zero bits.
 *
 * The module must be verified.
 */
#include "mir.h"

typedef struct Q {
  CsBuf *out;
  MirModule *m;
  const MirTarget *t;
  int ntemp;
  int nlabel;
  /* innermost loop */
  int loop_head;
  int loop_end;
  /* source locations: the file of QBE's last dbgfile, and the one of the
   * function's first loc, the only file its dbglocs can name */
  const char *file;
  const char *func_file;
  const MirStmt *loc; /* the statement whose line was given last, or NULL */
} Q;

static void emit(Q *q, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  cs_buf_vprintf(q->out, fmt, ap);
  va_end(ap);
}

static int new_temp(Q *q) {
  return q->ntemp++;
}

static int new_label(Q *q) {
  return q->nlabel++;
}

/* ---- type helpers ------------------------------------------------------ */

/* 'w' for integers of 32 bits or less, 'l' for 64-bit integers, uword and
 * pointers. */
static char cls(const MirType *t) {
  switch (t->kind) {
  case MT_U8:
  case MT_I8:
  case MT_U16:
  case MT_I16:
  case MT_U32:
  case MT_I32:
    return 'w';
  default:
    return 'l';
  }
}

static int bits_of(const MirType *t) {
  switch (t->kind) {
  case MT_U8:
  case MT_I8:
    return 8;
  case MT_U16:
  case MT_I16:
    return 16;
  case MT_U32:
  case MT_I32:
    return 32;
  default:
    return 64;
  }
}

/* The type a value has when it crosses a call: sub-word types keep their
 * extension. */
static const char *abi_type(const MirType *t) {
  switch (t->kind) {
  case MT_U8:
    return "ub";
  case MT_I8:
    return "sb";
  case MT_U16:
    return "uh";
  case MT_I16:
    return "sh";
  case MT_U32:
  case MT_I32:
    return "w";
  default:
    return "l";
  }
}

static const char *load_op(const MirType *t) {
  switch (t->kind) {
  case MT_U8:
    return "loadub";
  case MT_I8:
    return "loadsb";
  case MT_U16:
    return "loaduh";
  case MT_I16:
    return "loadsh";
  case MT_U32:
  case MT_I32:
    return "loadw";
  default:
    return "loadl";
  }
}

static const char *store_op(const MirType *t) {
  switch (bits_of(t)) {
  case 8:
    return "storeb";
  case 16:
    return "storeh";
  case 32:
    return "storew";
  default:
    return "storel";
  }
}

/* Prints an integer constant's bits as the decimal QBE reads back to the
 * same bits: sign-extended from the type's width for a signed type. */
static void emit_const(Q *q, const MirType *t, uint64_t v) {
  int bits = bits_of(t);
  if (mir_type_is_signed(t) && bits < 64 && (v & ((uint64_t)1 << (bits - 1)))) {
    emit(q, "-%" PRIu64, ((uint64_t)1 << bits) - v);
  } else if (v > (uint64_t)INT64_MAX) {
    emit(q, "-%" PRIu64, (uint64_t)0 - v);
  } else {
    emit(q, "%" PRIu64, v);
  }
}

/* Re-extends a sub-word result held in a 32-bit temporary. */
static int normalize(Q *q, const MirType *t, int v) {
  const char *op;
  int r;
  switch (t->kind) {
  case MT_U8:
    op = "extub";
    break;
  case MT_I8:
    op = "extsb";
    break;
  case MT_U16:
    op = "extuh";
    break;
  case MT_I16:
    op = "extsh";
    break;
  default:
    return v;
  }
  r = new_temp(q);
  emit(q, "\t%%t.%d =w %s %%t.%d\n", r, op, v);
  return r;
}

/* A 32-bit temporary that is nonzero exactly when v is. */
static int truth(Q *q, const MirType *t, int v) {
  int r;
  if (cls(t) == 'w') {
    return v;
  }
  r = new_temp(q);
  emit(q, "\t%%t.%d =w cnel %%t.%d, 0\n", r, v);
  return r;
}

/* ---- expressions ------------------------------------------------------- */

static int gen(Q *q, MirExpr *e);

/* QBE may move the trap's block anywhere, so it repeats its statement's line. */
static void gen_trap_if(Q *q, int cond) {
  int trap = new_label(q);
  int ok = new_label(q);
  emit(q, "\tjnz %%t.%d, @trap.%d, @ok.%d\n", cond, trap, ok);
  emit(q, "@trap.%d\n", trap);
  if (q->loc != NULL) {
    emit(q, "\tdbgloc %" PRIu32 ", %" PRIu32 "\n", q->loc->loc_line, q->loc->loc_col);
  }
  emit(q, "\thlt\n@ok.%d\n", ok);
}

static int gen_div(Q *q, MirExpr *e, int a, int b) {
  const MirType *t = e->type;
  char k = cls(t);
  bool is_signed = mir_type_is_signed(t);
  const char *op = e->op == ME_DIV ? (is_signed ? "div" : "udiv") : (is_signed ? "rem" : "urem");
  int zero = new_temp(q);
  int r;
  emit(q, "\t%%t.%d =w ceq%c %%t.%d, 0\n", zero, k, b);
  gen_trap_if(q, zero);
  if (is_signed) {
    /* MIN / -1 overflows: the same trap at every width */
    int bits = bits_of(t);
    int m1 = new_temp(q);
    int min = new_temp(q);
    int both = new_temp(q);
    emit(q, "\t%%t.%d =w ceq%c %%t.%d, -1\n", m1, k, b);
    emit(q, "\t%%t.%d =w ceq%c %%t.%d, ", min, k, a);
    emit_const(q, t, (uint64_t)1 << (bits - 1));
    emit(q, "\n\t%%t.%d =w and %%t.%d, %%t.%d\n", both, m1, min);
    gen_trap_if(q, both);
  }
  r = new_temp(q);
  emit(q, "\t%%t.%d =%c %s %%t.%d, %%t.%d\n", r, k, op, a, b);
  return normalize(q, t, r);
}

static int gen_shift(Q *q, MirExpr *e, int a, int b) {
  const MirType *t = e->type;
  char k = cls(t);
  const char *op = e->op == ME_SHL ? "shl" : (mir_type_is_signed(t) ? "sar" : "shr");
  int amount = new_temp(q);
  int r = new_temp(q);
  emit(q, "\t%%t.%d =w and %%t.%d, %d\n", amount, b, bits_of(t) - 1);
  emit(q, "\t%%t.%d =%c %s %%t.%d, %%t.%d\n", r, k, op, a, amount);
  return normalize(q, t, r);
}

static int gen_compare(Q *q, MirExpr *e, int a, int b) {
  const MirType *t = e->a->type;
  bool is_signed = mir_type_is_signed(t);
  const char *op;
  int r = new_temp(q);
  switch (e->op) {
  case ME_EQ:
    op = "eq";
    break;
  case ME_NE:
    op = "ne";
    break;
  case ME_LT:
    op = is_signed ? "slt" : "ult";
    break;
  case ME_LE:
    op = is_signed ? "sle" : "ule";
    break;
  case ME_GT:
    op = is_signed ? "sgt" : "ugt";
    break;
  default:
    op = is_signed ? "sge" : "uge";
    break;
  }
  emit(q, "\t%%t.%d =w c%s%c %%t.%d, %%t.%d\n", r, op, cls(t), a, b);
  return r;
}

/* Short-circuit: b is evaluated only when a does not decide the result. */
static int gen_logical(Q *q, MirExpr *e) {
  int r = new_temp(q);
  int rhs = new_label(q);
  int end = new_label(q);
  int a = truth(q, e->a->type, gen(q, e->a));
  int b;
  if (e->op == ME_LAND) {
    emit(q, "\t%%t.%d =w copy 0\n", r);
    emit(q, "\tjnz %%t.%d, @rhs.%d, @end.%d\n", a, rhs, end);
  } else {
    emit(q, "\t%%t.%d =w copy 1\n", r);
    emit(q, "\tjnz %%t.%d, @end.%d, @rhs.%d\n", a, end, rhs);
  }
  emit(q, "@rhs.%d\n", rhs);
  b = gen(q, e->b);
  emit(q, "\t%%t.%d =w cne%c %%t.%d, 0\n", r, cls(e->b->type), b);
  emit(q, "@end.%d\n", end);
  return r;
}

static int gen_as(Q *q, MirExpr *e) {
  const MirType *from = e->a->type;
  const MirType *to = e->targ;
  int a = gen(q, e->a);
  int r;
  if (cls(from) == cls(to)) {
    /* same register width: a sub-word target still needs its extension */
    if (bits_of(to) < 32 && !(from->kind == to->kind)) {
      return normalize(q, to, a);
    }
    return a;
  }
  r = new_temp(q);
  if (cls(to) == 'l') {
    /* widening extends by the source's signedness */
    emit(q, "\t%%t.%d =l %s %%t.%d\n", r, mir_type_is_signed(from) ? "extsw" : "extuw", a);
    return r;
  }
  /* narrowing truncates */
  emit(q, "\t%%t.%d =w copy %%t.%d\n", r, a);
  return normalize(q, to, r);
}

static int gen(Q *q, MirExpr *e) {
  int r;
  int a;
  int b;
  size_t i;
  switch (e->op) {
  case ME_CONST:
    r = new_temp(q);
    emit(q, "\t%%t.%d =%c copy ", r, cls(e->type));
    emit_const(q, e->type, e->ival);
    emit(q, "\n");
    return r;

  case ME_LOCAL:
    r = new_temp(q);
    emit(q, "\t%%t.%d =%c %s %%v.%d\n", r, cls(e->type), load_op(e->type), e->local->slot);
    return r;

  case ME_GLOBAL:
    r = new_temp(q);
    emit(q, "\t%%t.%d =l copy $%s\n", r, e->name);
    return r;

  case ME_ADDR:
    r = new_temp(q);
    emit(q, "\t%%t.%d =l copy %%v.%d\n", r, e->local->slot);
    return r;

  case ME_LOAD:
    a = gen(q, e->a);
    r = new_temp(q);
    emit(q, "\t%%t.%d =%c %s %%t.%d\n", r, cls(e->type), load_op(e->type), a);
    return r;

  case ME_INDEX: {
    uint64_t size = mir_sizeof(q->t, e->a->type->elem);
    int idx;
    a = gen(q, e->a);
    b = gen(q, e->b);
    idx = b;
    if (cls(e->b->type) == 'w') {
      idx = new_temp(q);
      emit(q, "\t%%t.%d =l %s %%t.%d\n", idx, mir_type_is_signed(e->b->type) ? "extsw" : "extuw", b);
    }
    if (size != 1) {
      int scaled = new_temp(q);
      emit(q, "\t%%t.%d =l mul %%t.%d, %" PRIu64 "\n", scaled, idx, size);
      idx = scaled;
    }
    r = new_temp(q);
    emit(q, "\t%%t.%d =l add %%t.%d, %%t.%d\n", r, a, idx);
    return r;
  }

  case ME_FIELD: {
    MirStruct *st = e->a->type->elem->st;
    uint64_t off;
    a = gen(q, e->a);
    (void)mir_sizeof(q->t, &st->type); /* lays the struct out */
    off = st->fields[e->field_index].offset;
    if (off == 0) {
      return a;
    }
    r = new_temp(q);
    emit(q, "\t%%t.%d =l add %%t.%d, %" PRIu64 "\n", r, a, off);
    return r;
  }

  case ME_CALL: {
    int *args = (int *)cs_arena_new(&q->m->arena, cs_mul_size(e->nargs == 0 ? 1 : e->nargs, sizeof(int)));
    for (i = 0; i < e->nargs; i++) {
      args[i] = gen(q, e->args[i]);
    }
    r = -1;
    emit(q, "\t");
    if (e->type->kind != MT_VOID) {
      r = new_temp(q);
      emit(q, "%%t.%d =%s ", r, abi_type(e->type));
    }
    emit(q, "call $%s(", e->name);
    for (i = 0; i < e->nargs; i++) {
      emit(q, "%s%s %%t.%d", i == 0 ? "" : ", ", abi_type(e->args[i]->type), args[i]);
    }
    emit(q, ")\n");
    return r;
  }

  case ME_ADD:
  case ME_SUB:
  case ME_MUL:
  case ME_AND:
  case ME_OR:
  case ME_XOR:
    a = gen(q, e->a);
    b = gen(q, e->b);
    r = new_temp(q);
    emit(q, "\t%%t.%d =%c %s %%t.%d, %%t.%d\n", r, cls(e->type), mir_op_name(e->op), a, b);
    return normalize(q, e->type, r);

  case ME_DIV:
  case ME_REM:
    a = gen(q, e->a);
    b = gen(q, e->b);
    return gen_div(q, e, a, b);

  case ME_SHL:
  case ME_SHR:
    a = gen(q, e->a);
    b = gen(q, e->b);
    return gen_shift(q, e, a, b);

  case ME_NOT:
    a = gen(q, e->a);
    r = new_temp(q);
    emit(q, "\t%%t.%d =%c xor %%t.%d, -1\n", r, cls(e->type), a);
    return normalize(q, e->type, r);

  case ME_NEG:
    a = gen(q, e->a);
    r = new_temp(q);
    emit(q, "\t%%t.%d =%c neg %%t.%d\n", r, cls(e->type), a);
    return normalize(q, e->type, r);

  case ME_EQ:
  case ME_NE:
  case ME_LT:
  case ME_LE:
  case ME_GT:
  case ME_GE:
    a = gen(q, e->a);
    b = gen(q, e->b);
    return gen_compare(q, e, a, b);

  case ME_LAND:
  case ME_LOR:
    return gen_logical(q, e);

  case ME_LNOT:
    a = gen(q, e->a);
    r = new_temp(q);
    emit(q, "\t%%t.%d =w ceq%c %%t.%d, 0\n", r, cls(e->a->type), a);
    return r;

  case ME_AS:
    return gen_as(q, e);

  case ME_SIZEOF:
  case ME_ALIGNOF:
    r = new_temp(q);
    emit(q, "\t%%t.%d =l copy %" PRIu64 "\n", r,
         e->op == ME_SIZEOF ? mir_sizeof(q->t, e->targ) : mir_alignof(q->t, e->targ));
    return r;

  case ME_OP_COUNT:
    break;
  }
  CS_BUG("unknown operation");
  return -1;
}

/* ---- source locations -------------------------------------------------- */

/* The file of the first loc in a block, in text order, or NULL. */
static const char *first_loc_file(const MirStmt *s) {
  const char *file;
  for (; s != NULL; s = s->next) {
    if (s->loc_line != 0) {
      return s->loc_file;
    }
    file = first_loc_file(s->body);
    if (file == NULL) {
      file = first_loc_file(s->els);
    }
    if (file != NULL) {
      return file;
    }
  }
  return NULL;
}

/* QBE passes the string to the assembler as written. */
static void gen_dbgfile(Q *q, const char *file) {
  emit(q, "dbgfile ");
  mir_asm_string(q->out, file);
  emit(q, "\n");
}

/* QBE cannot change files within a function: a statement located in
 * another file than the function's first loc gets no line. */
static void gen_loc(Q *q, const MirStmt *s) {
  q->loc = NULL;
  if (s->loc_line != 0 && strcmp(s->loc_file, q->func_file) == 0) {
    emit(q, "\tdbgloc %" PRIu32 ", %" PRIu32 "\n", s->loc_line, s->loc_col);
    q->loc = s;
  }
}

/* ---- statements -------------------------------------------------------- */

static void gen_block(Q *q, MirStmt *s);

/* Stores zero over a local's slot. */
static void gen_zero(Q *q, MirLocal *l) {
  uint64_t size = mir_sizeof(q->t, l->type);
  uint64_t align = mir_alignof(q->t, l->type);
  uint64_t step = (align >= 8 && size % 8 == 0) ? 8 : 1;
  const char *op = step == 8 ? "storel" : "storeb";
  if (mir_type_is_scalar(l->type)) {
    emit(q, "\t%s 0, %%v.%d\n", store_op(l->type), l->slot);
    return;
  }
  if (size == 0) {
    return;
  }
  {
    int i = new_temp(q);
    int c = new_temp(q);
    int p = new_temp(q);
    int head = new_label(q);
    int body = new_label(q);
    int end = new_label(q);
    emit(q, "\t%%t.%d =l copy 0\n", i);
    emit(q, "@zero.%d\n", head);
    emit(q, "\t%%t.%d =w cultl %%t.%d, %" PRIu64 "\n", c, i, size);
    emit(q, "\tjnz %%t.%d, @zero.%d, @zero.%d\n", c, body, end);
    emit(q, "@zero.%d\n", body);
    emit(q, "\t%%t.%d =l add %%v.%d, %%t.%d\n", p, l->slot, i);
    emit(q, "\t%s 0, %%t.%d\n", op, p);
    emit(q, "\t%%t.%d =l add %%t.%d, %" PRIu64 "\n", i, i, step);
    emit(q, "\tjmp @zero.%d\n", head);
    emit(q, "@zero.%d\n", end);
  }
}

static void gen_stmt(Q *q, MirStmt *s) {
  int a;
  int b;
  /* a while's line goes after its label, with each test of the condition */
  if (s->kind != MS_WHILE) {
    gen_loc(q, s);
  }
  switch (s->kind) {
  case MS_LOCAL:
    if (s->a != NULL) {
      a = gen(q, s->a);
      emit(q, "\t%s %%t.%d, %%v.%d\n", store_op(s->type), a, s->local->slot);
    } else {
      gen_zero(q, s->local);
    }
    return;

  case MS_SET:
    a = gen(q, s->a);
    emit(q, "\t%s %%t.%d, %%v.%d\n", store_op(s->local->type), a, s->local->slot);
    return;

  case MS_STORE:
    a = gen(q, s->a);
    b = gen(q, s->b);
    emit(q, "\t%s %%t.%d, %%t.%d\n", store_op(s->b->type), b, a);
    return;

  case MS_EVAL:
    (void)gen(q, s->a);
    return;

  case MS_IF: {
    int then = new_label(q);
    int els = new_label(q);
    int fi = new_label(q);
    a = truth(q, s->a->type, gen(q, s->a));
    emit(q, "\tjnz %%t.%d, @then.%d, @else.%d\n", a, then, els);
    emit(q, "@then.%d\n", then);
    gen_block(q, s->body);
    emit(q, "\tjmp @fi.%d\n", fi);
    emit(q, "@else.%d\n", els);
    gen_block(q, s->els);
    emit(q, "@fi.%d\n", fi);
    return;
  }

  case MS_WHILE: {
    int head = new_label(q);
    int body = new_label(q);
    int end = new_label(q);
    int saved_head = q->loop_head;
    int saved_end = q->loop_end;
    emit(q, "@loop.%d\n", head);
    gen_loc(q, s);
    a = truth(q, s->a->type, gen(q, s->a));
    emit(q, "\tjnz %%t.%d, @body.%d, @done.%d\n", a, body, end);
    emit(q, "@body.%d\n", body);
    q->loop_head = head;
    q->loop_end = end;
    gen_block(q, s->body);
    q->loop_head = saved_head;
    q->loop_end = saved_end;
    emit(q, "\tjmp @loop.%d\n", head);
    emit(q, "@done.%d\n", end);
    return;
  }

  /* A jump ends its block; what follows starts an unreachable one. */
  case MS_BREAK:
    emit(q, "\tjmp @done.%d\n@dead.%d\n", q->loop_end, new_label(q));
    return;

  case MS_CONTINUE:
    emit(q, "\tjmp @loop.%d\n@dead.%d\n", q->loop_head, new_label(q));
    return;

  case MS_RETURN:
    if (s->a != NULL) {
      a = gen(q, s->a);
      emit(q, "\tret %%t.%d\n@dead.%d\n", a, new_label(q));
    } else {
      emit(q, "\tret\n@dead.%d\n", new_label(q));
    }
    return;
  }
  CS_BUG("unknown statement");
}

static void gen_block(Q *q, MirStmt *s) {
  for (; s != NULL; s = s->next) {
    gen_stmt(q, s);
  }
}

/* ---- declarations ------------------------------------------------------ */

static void gen_func(Q *q, MirFunc *f) {
  MirLocal *l;
  size_t i;
  q->ntemp = 0;
  q->nlabel = 0;
  q->loc = NULL;
  q->func_file = first_loc_file(f->body);
  if (q->func_file != NULL && (q->file == NULL || strcmp(q->file, q->func_file) != 0)) {
    gen_dbgfile(q, q->func_file);
    q->file = q->func_file;
  }
  emit(q, "%sfunction ", f->is_export ? "export " : "");
  if (f->ret->kind != MT_VOID) {
    emit(q, "%s ", abi_type(f->ret));
  }
  emit(q, "$%s(", f->name);
  for (i = 0; i < f->nparams; i++) {
    emit(q, "%s%s %%p.%lu", i == 0 ? "" : ", ", abi_type(f->params[i].type), (unsigned long)i);
  }
  emit(q, ") {\n@start\n");
  /* every slot is allocated once, in the entry block */
  for (l = f->locals; l != NULL; l = l->next) {
    uint64_t size = mir_sizeof(q->t, l->type);
    uint64_t align = mir_alignof(q->t, l->type);
    emit(q, "\t%%v.%d =l alloc%d %" PRIu64 "\n", l->slot, align > 8 ? 16 : align > 4 ? 8 : 4, size == 0 ? 1 : size);
  }
  i = 0;
  for (l = f->locals; l != NULL; l = l->next) {
    if (l->is_param) {
      emit(q, "\t%s %%p.%lu, %%v.%d\n", store_op(l->type), (unsigned long)i, l->slot);
      i++;
    }
  }
  gen_block(q, f->body);
  /* the verifier proved a non-void function never gets here */
  emit(q, "%s", f->ret->kind == MT_VOID ? "\tret\n" : "\thlt\n");
  emit(q, "}\n\n");
}

static void gen_global(Q *q, MirGlobal *g) {
  uint64_t size = mir_sizeof(q->t, g->type);
  size_t i;
  emit(q, "data $%s = align %" PRIu64 " { ", g->name, mir_alignof(q->t, g->type));
  switch (g->init) {
  case MI_ZERO:
    emit(q, "z %" PRIu64, size == 0 ? 1 : size);
    break;
  case MI_INT: {
    static const char letters[] = {'b', 'h', 'w', 'l'};
    int bits = bits_of(g->type);
    emit(q, "%c ", letters[bits == 8 ? 0 : bits == 16 ? 1 : bits == 32 ? 2 : 3]);
    emit_const(q, g->type, g->ival);
    break;
  }
  case MI_BYTES:
    if (g->nbytes == 0) {
      emit(q, "z 1");
    } else {
      emit(q, "b");
      for (i = 0; i < g->nbytes; i++) {
        emit(q, " %u", (unsigned)(unsigned char)g->bytes[i]);
      }
    }
    break;
  case MI_ADDR:
    emit(q, "l $%s", g->addr_name);
    break;
  }
  emit(q, " }\n");
}

const char *mir_lower_qbe(CsBuf *out, MirModule *m, const MirTarget *t) {
  Q q;
  MirGlobal *g;
  MirFunc *f;
  const char *first_file = NULL;
  if (!m->verified) {
    CS_BUG("lowering an unverified module");
  }
  memset(&q, 0, sizeof q);
  q.out = out;
  q.m = m;
  q.t = t;
  for (g = m->globals; g != NULL; g = g->next) {
    gen_global(&q, g);
  }
  if (m->globals != NULL) {
    emit(&q, "\n");
  }
  for (f = m->funcs; f != NULL; f = f->next) {
    if (!f->is_extern) {
      gen_func(&q, f);
      if (first_file == NULL) {
        first_file = q.file;
      }
    }
  }
  return first_file;
}
