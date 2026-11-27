/* CodeView variables, lexical scopes and their types (cesiumlang/cesium#143).
 *
 * Each local found in memory (mir_debug_scan.c) gets an S_REGREL32, at its
 * offset from rbp or rsp, in the S_BLOCK32 of its scope, nested as the
 * scopes are, between its function's S_GPROC32 and S_END. Its type is a
 * record of .debug$T, which needs no relocation and is written as bytes: the
 * integers and pointers to them are CodeView's own types; a struct is
 * declared forward first, so that a pointer to it, even in its own fields,
 * refers to the declaration, which a debugger resolves by the name.
 */
#include "mir.h"

#define CV_SIGNATURE_C13 4
#define S_END 0x0006
#define S_BLOCK32 0x1103
#define S_REGREL32 0x1111
#define LF_POINTER 0x1002
#define LF_FIELDLIST 0x1203
#define LF_ARRAY 0x1503
#define LF_STRUCTURE 0x1505
#define LF_MEMBER 0x150d
#define LF_ULONG 0x8004
#define LF_UQUADWORD 0x800a
#define CV_PROP_FWDREF 0x0080
#define CV_MEMBER_PUBLIC 3
#define CV_PTR_64 0x0c
#define CV_AMD64_RBP 334
#define CV_AMD64_RSP 335
/* the built-in types, and pointers to them by adding T_64P */
#define T_VOID 0x0003
#define T_CHAR 0x0010
#define T_SHORT 0x0011
#define T_QUAD 0x0013
#define T_UCHAR 0x0020
#define T_USHORT 0x0021
#define T_UQUAD 0x0023
#define T_REAL32 0x0040
#define T_REAL64 0x0041
#define T_INT4 0x0074
#define T_UINT4 0x0075
#define T_64P 0x0600

typedef struct TypeEntry {
  const MirType *type;
  uint32_t index;
  uint32_t fwd; /* a struct's forward declaration */
} TypeEntry;

void mir_cv_types_init(MirCvTypes *types) {
  memset(types, 0, sizeof *types);
  types->next = 0x1000;
}

void mir_cv_types_free(MirCvTypes *types) {
  cs_buf_free(&types->map);
  cs_buf_free(&types->records);
}

static void put16(CsBuf *b, uint32_t v) {
  cs_buf_putc(b, (char)(v & 0xff));
  cs_buf_putc(b, (char)((v >> 8) & 0xff));
}

static void put32(CsBuf *b, uint32_t v) {
  put16(b, v & 0xffff);
  put16(b, v >> 16);
}

/* A numeric leaf: the value itself below 0x8000, else a typed one. */
static void put_number(CsBuf *b, uint64_t v) {
  if (v < 0x8000) {
    put16(b, (uint32_t)v);
  } else if (v <= 0xffffffffu) {
    put16(b, LF_ULONG);
    put32(b, (uint32_t)v);
  } else {
    put16(b, LF_UQUADWORD);
    put32(b, (uint32_t)(v & 0xffffffffu));
    put32(b, (uint32_t)(v >> 32));
  }
}

/* Pads to four bytes with LF_PAD bytes, which readers skip. */
static void pad4(CsBuf *b, size_t start) {
  while ((b->len - start) % 4 != 0) {
    cs_buf_putc(b, (char)(0xf0 + 4 - (b->len - start) % 4));
  }
}

/* Appends a record of kind with data, and gives it the next index. */
static uint32_t add_record(MirCvTypes *types, uint32_t kind, const CsBuf *data) {
  size_t start;
  put16(&types->records, (uint32_t)(2 + data->len + (4 - (2 + 2 + data->len) % 4) % 4));
  start = types->records.len;
  put16(&types->records, kind);
  cs_buf_put(&types->records, data->data == NULL ? "" : data->data, data->len);
  /* the record with its length is a multiple of four */
  while ((types->records.len - start + 2) % 4 != 0) {
    cs_buf_putc(&types->records, (char)(0xf0 + 4 - (types->records.len - start + 2) % 4));
  }
  return types->next++;
}

static TypeEntry *find_type(MirCvTypes *types, const MirType *t) {
  TypeEntry *e = (TypeEntry *)types->map.data;
  size_t n = types->map.len / sizeof(TypeEntry);
  size_t i;
  for (i = 0; i < n; i++) {
    if (e[i].type == t || mir_type_eq(e[i].type, t)) {
      return &e[i];
    }
  }
  return NULL;
}

static void remember(MirCvTypes *types, const MirType *t, uint32_t index, uint32_t fwd) {
  TypeEntry e;
  e.type = t;
  e.index = index;
  e.fwd = fwd;
  cs_buf_put(&types->map, (const char *)&e, sizeof e);
}

static uint32_t prim(const MirTarget *target, const MirType *t) {
  switch (t->kind) {
  case MT_VOID:
    return T_VOID;
  case MT_U8:
    return T_UCHAR;
  case MT_I8:
    return T_CHAR;
  case MT_U16:
    return T_USHORT;
  case MT_I16:
    return T_SHORT;
  case MT_U32:
    return T_UINT4;
  case MT_I32:
    return T_INT4;
  case MT_U64:
    return T_UQUAD;
  case MT_I64:
    return T_QUAD;
  case MT_UWORD:
    return target->ptr_size == 8 ? T_UQUAD : T_UINT4;
  case MT_F32:
    return T_REAL32;
  case MT_F64:
    return T_REAL64;
  default:
    return 0;
  }
}

static uint32_t type_index(MirCvTypes *types, const MirTarget *target, MirType *t);

/* A struct: its forward declaration, its fields, then its definition. */
static uint32_t struct_index(MirCvTypes *types, const MirTarget *target, MirType *t) {
  MirStruct *st = t->st;
  CsBuf data = {NULL, 0, 0};
  CsBuf fields = {NULL, 0, 0};
  uint32_t fwd;
  uint32_t list;
  uint32_t full;
  size_t i;
  put16(&data, 0);
  put16(&data, CV_PROP_FWDREF);
  put32(&data, 0);
  put32(&data, 0);
  put32(&data, 0);
  put_number(&data, 0);
  cs_buf_put(&data, st->name, strlen(st->name) + 1);
  fwd = add_record(types, LF_STRUCTURE, &data);
  remember(types, t, fwd, fwd);
  (void)mir_sizeof(target, t); /* lays out the fields */
  for (i = 0; i < st->nfields; i++) {
    uint32_t ft = type_index(types, target, st->fields[i].type);
    size_t start = fields.len;
    put16(&fields, LF_MEMBER);
    put16(&fields, CV_MEMBER_PUBLIC);
    put32(&fields, ft);
    put_number(&fields, st->fields[i].offset);
    cs_buf_put(&fields, st->fields[i].name, strlen(st->fields[i].name) + 1);
    pad4(&fields, start);
  }
  list = add_record(types, LF_FIELDLIST, &fields);
  data.len = 0;
  put16(&data, (uint32_t)st->nfields);
  put16(&data, 0);
  put32(&data, list);
  put32(&data, 0);
  put32(&data, 0);
  put_number(&data, mir_sizeof(target, t));
  cs_buf_put(&data, st->name, strlen(st->name) + 1);
  full = add_record(types, LF_STRUCTURE, &data);
  find_type(types, t)->index = full;
  cs_buf_free(&data);
  cs_buf_free(&fields);
  return full;
}

static uint32_t type_index(MirCvTypes *types, const MirTarget *target, MirType *t) {
  TypeEntry *e = find_type(types, t);
  CsBuf data = {NULL, 0, 0};
  uint32_t index;
  if (prim(target, t) != 0) {
    return prim(target, t);
  }
  if (e != NULL) {
    return e->index;
  }
  if (t->kind == MT_STRUCT) {
    return struct_index(types, target, t);
  }
  if (t->kind == MT_PTR) {
    uint32_t to = prim(target, t->elem);
    if (to != 0) {
      return T_64P | to;
    }
    /* a struct is pointed to by its forward declaration */
    if (t->elem->kind == MT_STRUCT) {
      (void)type_index(types, target, t->elem);
      to = find_type(types, t->elem)->fwd;
    } else {
      to = type_index(types, target, t->elem);
    }
    /* the struct's own fields may have made this pointer */
    e = find_type(types, t);
    if (e != NULL) {
      return e->index;
    }
    put32(&data, to);
    put32(&data, CV_PTR_64 | (8u << 13));
    index = add_record(types, LF_POINTER, &data);
  } else {
    uint32_t elem = type_index(types, target, t->elem);
    put32(&data, elem);
    put32(&data, T_UQUAD);
    put_number(&data, mir_sizeof(target, t));
    cs_buf_putc(&data, '\0');
    index = add_record(types, LF_ARRAY, &data);
  }
  remember(types, t, index, 0);
  cs_buf_free(&data);
  return index;
}

/* A record of .debug$S, its length from labels. */
static void begin_symbol(CsBuf *out, uint32_t kind, uint32_t id, int n) {
  cs_buf_printf(out, "\t.short .Lcs_sy%" PRIu32 "_%d_end - .Lcs_sy%" PRIu32 "_%d\n", id, n, id, n);
  cs_buf_printf(out, ".Lcs_sy%" PRIu32 "_%d:\n\t.short 0x%04x\n", id, n, kind);
}

static void end_symbol(CsBuf *out, uint32_t id, int n) {
  cs_buf_printf(out, "\t.p2align 2\n.Lcs_sy%" PRIu32 "_%d_end:\n", id, n);
}

/* The locals of scope k; n numbers the records. */
static void put_locals(CsBuf *out, MirCvTypes *types, const MirVarsFunc *f, const MirTarget *t, int k, int *n) {
  size_t i;
  for (i = 0; i < f->d.nlocals; i++) {
    const MirDebugVar *v = &f->vars[i];
    if (f->d.scope[i] != k || v->base == MIR_BASE_NONE) {
      continue;
    }
    begin_symbol(out, S_REGREL32, f->id, *n);
    cs_buf_printf(out, "\t.long %" PRId64 "\n\t.long 0x%" PRIx32 "\n\t.short %d\n\t.asciz ", v->offset,
                  type_index(types, t, f->d.locals[i]->type), v->base == MIR_BASE_FP ? CV_AMD64_RBP : CV_AMD64_RSP);
    mir_asm_string(out, f->d.locals[i]->name);
    cs_buf_putc(out, '\n');
    end_symbol(out, f->id, (*n)++);
  }
}

/* An S_BLOCK32 from .Lcs_{b}ID_J to .Lcs_{e}ID_J; S_END closes it. */
static void put_block(CsBuf *out, const MirVarsFunc *f, const char *b, const char *e, int j, int *n) {
  /* parent and end: the linker fills them in */
  begin_symbol(out, S_BLOCK32, f->id, *n);
  cs_buf_printf(out, "\t.long 0, 0\n\t.long .Lcs_%s%" PRIu32 "_%d - .Lcs_%s%" PRIu32 "_%d\n", e, f->id, j, b,
                f->id, j);
  cs_buf_printf(out, "\t.secrel32 .Lcs_%s%" PRIu32 "_%d\n\t.secidx .Lcs_%s%" PRIu32 "_%d\n\t.byte 0\n", b, f->id,
                j, b, f->id, j);
  end_symbol(out, f->id, (*n)++);
}

static void put_end(CsBuf *out) {
  cs_buf_printf(out, "\t.short 2\n\t.short 0x%04x\n", S_END);
}

/* The locals of scope k, then its nested scopes. */
static void put_scope(CsBuf *out, MirCvTypes *types, const MirVarsFunc *f, const MirTarget *t, int k, int *n) {
  size_t j;
  put_locals(out, types, f, t, k, n);
  for (j = 1; j < f->d.nscopes; j++) {
    if (f->d.parent[j] != k || !f->opened[j]) {
      continue;
    }
    put_block(out, f, "sb", "se", (int)j, n);
    put_scope(out, types, f, t, (int)j, n);
    put_end(out);
  }
}

void mir_cv_put_vars(CsBuf *out, MirCvTypes *types, const MirVarsFunc *f, const MirTarget *t) {
  CsArena a;
  int *chain;
  int n = 0;
  size_t j;
  memset(&a, 0, sizeof a);
  chain = cs_arena_new(&a, cs_mul_size(f->d.nscopes, sizeof *chain));
  put_scope(out, types, f, t, 0, &n);
  /* a trap's block, out of its scopes: their locals again, nested */
  for (j = 0; j < mir_vars_ntraps(f); j++) {
    size_t len = mir_vars_chain(f, j, chain);
    size_t i;
    for (i = 0; i < len; i++) {
      put_block(out, f, "tb", "te", (int)j, &n);
      put_locals(out, types, f, t, chain[i], &n);
    }
    for (i = 0; i < len; i++) {
      put_end(out);
    }
  }
  cs_arena_free(&a);
}

void mir_cv_put_types(CsBuf *out, const MirCvTypes *types) {
  size_t i;
  if (types->next == 0x1000) {
    return;
  }
  cs_buf_printf(out, ".section .debug$T,\"dr\"\n\t.p2align 2\n\t.long %d\n", CV_SIGNATURE_C13);
  for (i = 0; i < types->records.len; i++) {
    cs_buf_printf(out, "%s0x%02x", i % 16 == 0 ? "\t.byte " : ", ", (unsigned)(unsigned char)types->records.data[i]);
    if (i % 16 == 15 || i + 1 == types->records.len) {
      cs_buf_putc(out, '\n');
    }
  }
}
