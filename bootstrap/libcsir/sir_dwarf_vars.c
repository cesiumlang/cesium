/* DWARF variables, lexical scopes and their types (cesiumlang/cesium#143).
 *
 * In a build that keeps variables, QBE's assembly for ELF and Mach-O is
 * rewritten as COFF's is: each function gets labels at its start and end,
 * each scope its labels where its markers are (sir_debug.c), and the compile
 * unit of sir_dwarf.c gets children: a DW_TAG_subprogram for each function,
 * a DW_TAG_formal_parameter or DW_TAG_variable for each local found in
 * memory, at its offset from the frame or stack pointer (DW_OP_breg), in a
 * DW_TAG_lexical_block for each scope, and the types they name. A reference
 * to a type is its offset in the unit, computed by the assembler. Strings
 * are DW_FORM_strp and lengths of code are computed with .set, as in
 * sir_dwarf.c, for Mach-O.
 */
#include "sir_core.h"

/* DWARF 4, section 7 */
#define DW_TAG_array_type 0x01
#define DW_TAG_formal_parameter 0x05
#define DW_TAG_lexical_block 0x0b
#define DW_TAG_member 0x0d
#define DW_TAG_pointer_type 0x0f
#define DW_TAG_structure_type 0x13
#define DW_TAG_subrange_type 0x21
#define DW_TAG_base_type 0x24
#define DW_TAG_subprogram 0x2e
#define DW_TAG_variable 0x34
#define DW_AT_location 0x02
#define DW_AT_name 0x03
#define DW_AT_byte_size 0x0b
#define DW_AT_low_pc 0x11
#define DW_AT_high_pc 0x12
#define DW_AT_count 0x37
#define DW_AT_data_member_location 0x38
#define DW_AT_encoding 0x3e
#define DW_AT_type 0x49
#define DW_FORM_addr 0x01
#define DW_FORM_data4 0x06
#define DW_FORM_data8 0x07
#define DW_FORM_data1 0x0b
#define DW_FORM_strp 0x0e
#define DW_FORM_ref4 0x13
#define DW_FORM_exprloc 0x18
#define DW_ATE_float 0x04
#define DW_ATE_signed 0x05
#define DW_ATE_unsigned 0x07
#define DW_ATE_unsigned_char 0x08
#define DW_OP_breg0 0x70

/* the abbreviations after the compile unit's, 1 */
enum { AB_BASE = 2, AB_PTR, AB_PTR_VOID, AB_STRUCT, AB_MEMBER, AB_ARRAY, AB_SUBRANGE, AB_SUBPROGRAM, AB_VARIABLE,
       AB_PARAM, AB_BLOCK };

typedef struct Dw {
  const SirTarget *t;
  const char *p; /* the prefix of local labels */
  SirDwarfExtra *x;
  CsBuf types;   /* the SirType pointers given a label, cs_tyN */
  int nstr;
  int ndiff;
} Dw;

static void abbrev(CsBuf *out, int code, int tag, bool children, const int *attrs, size_t n) {
  size_t i;
  cs_buf_printf(out, "\t.byte %d, 0x%02x, %d\n", code, tag, children);
  for (i = 0; i < n; i++) {
    cs_buf_printf(out, "\t.byte 0x%02x, 0x%02x\n", attrs[2 * i], attrs[2 * i + 1]);
  }
  cs_buf_puts(out, "\t.byte 0, 0\n");
}

static void put_abbrevs(CsBuf *out) {
  static const int base[] = {DW_AT_name, DW_FORM_strp, DW_AT_encoding, DW_FORM_data1, DW_AT_byte_size, DW_FORM_data1};
  static const int ptr[] = {DW_AT_byte_size, DW_FORM_data1, DW_AT_type, DW_FORM_ref4};
  static const int st[] = {DW_AT_name, DW_FORM_strp, DW_AT_byte_size, DW_FORM_data4};
  static const int member[] = {DW_AT_name, DW_FORM_strp, DW_AT_type, DW_FORM_ref4, DW_AT_data_member_location,
                               DW_FORM_data4};
  static const int array[] = {DW_AT_type, DW_FORM_ref4};
  static const int range[] = {DW_AT_count, DW_FORM_data8};
  static const int sub[] = {DW_AT_name, DW_FORM_strp, DW_AT_low_pc, DW_FORM_addr, DW_AT_high_pc, DW_FORM_data4};
  static const int var[] = {DW_AT_name, DW_FORM_strp, DW_AT_type, DW_FORM_ref4, DW_AT_location, DW_FORM_exprloc};
  static const int block[] = {DW_AT_low_pc, DW_FORM_addr, DW_AT_high_pc, DW_FORM_data4};
  abbrev(out, AB_BASE, DW_TAG_base_type, false, base, 3);
  abbrev(out, AB_PTR, DW_TAG_pointer_type, false, ptr, 2);
  abbrev(out, AB_PTR_VOID, DW_TAG_pointer_type, false, ptr, 1);
  abbrev(out, AB_STRUCT, DW_TAG_structure_type, true, st, 2);
  abbrev(out, AB_MEMBER, DW_TAG_member, false, member, 3);
  abbrev(out, AB_ARRAY, DW_TAG_array_type, true, array, 1);
  abbrev(out, AB_SUBRANGE, DW_TAG_subrange_type, false, range, 1);
  abbrev(out, AB_SUBPROGRAM, DW_TAG_subprogram, true, sub, 3);
  abbrev(out, AB_VARIABLE, DW_TAG_variable, false, var, 3);
  abbrev(out, AB_PARAM, DW_TAG_formal_parameter, false, var, 3);
  abbrev(out, AB_BLOCK, DW_TAG_lexical_block, true, block, 2);
}

/* A string of .debug_str, by its offset. */
static void put_strp(Dw *w, const char *s) {
  int n = w->nstr++;
  cs_buf_printf(&w->x->str, "%scs_s%d:\n\t.asciz ", w->p, n);
  sir_asm_string(&w->x->str, s);
  cs_buf_putc(&w->x->str, '\n');
  if (w->t->format == SIR_MACHO) {
    cs_buf_printf(&w->x->info, "\t.set %scs_s%d_off, %scs_s%d - %scs_producer\n\t.long %scs_s%d_off\n", w->p, n, w->p,
                  n, w->p, w->p, n);
  } else {
    cs_buf_printf(&w->x->info, "\t.long %scs_s%d\n", w->p, n);
  }
}

/* The distance between two labels of the text, as a data4. */
static void put_diff(Dw *w, const char *end, const char *start) {
  int n = w->ndiff++;
  cs_buf_printf(&w->x->info, "\t.set %scs_d%d, %s - %s\n\t.long %scs_d%d\n", w->p, n, end, start, w->p, n);
}

static void put_addr(Dw *w, const char *label) {
  cs_buf_printf(&w->x->info, "\t%s %s\n", w->t->ptr_size == 8 ? ".quad" : ".long", label);
}

/* A reference to a type: its label's offset in the unit. */
static void put_type_ref(Dw *w, SirType *type) {
  SirType **e = (SirType **)w->types.data;
  size_t n = w->types.len / sizeof(SirType *);
  size_t i;
  for (i = 0; i < n; i++) {
    if (e[i] == type || sir_type_eq(e[i], type)) {
      break;
    }
  }
  if (i == n) {
    cs_buf_put(&w->types, (const char *)&type, sizeof(SirType *));
  }
  cs_buf_printf(&w->x->info, "\t.long %scs_ty%d - %scs_cu\n", w->p, (int)i, w->p);
}

static void base_type(Dw *w, SirType *type) {
  CsBuf name = {NULL, 0, 0};
  int enc = sir_type_is_signed(type) ? DW_ATE_signed : DW_ATE_unsigned;
  if (type->kind == MT_U8) {
    enc = DW_ATE_unsigned_char;
  } else if (type->kind == MT_F32 || type->kind == MT_F64) {
    enc = DW_ATE_float;
  }
  sir_type_print(&name, type);
  cs_buf_printf(&w->x->info, "\t.byte %d\n", AB_BASE);
  put_strp(w, name.data);
  cs_buf_printf(&w->x->info, "\t.byte 0x%02x, %d\n", enc, (int)sir_sizeof(w->t, type));
  cs_buf_free(&name);
}

/* The DIE of type i, labeled, which may give further types labels. */
static void put_type(Dw *w, size_t i) {
  SirType *type = ((SirType **)w->types.data)[i];
  CsBuf *info = &w->x->info;
  size_t k;
  cs_buf_printf(info, "%scs_ty%d:\n", w->p, (int)i);
  if (type->kind == MT_PTR) {
    bool to_void = type->elem->kind == MT_VOID;
    cs_buf_printf(info, "\t.byte %d\n\t.byte %d\n", to_void ? AB_PTR_VOID : AB_PTR, (int)w->t->ptr_size);
    if (!to_void) {
      put_type_ref(w, type->elem);
    }
  } else if (type->kind == MT_STRUCT) {
    SirStruct *st = type->st;
    cs_buf_printf(info, "\t.byte %d\n", AB_STRUCT);
    put_strp(w, st->name);
    cs_buf_printf(info, "\t.long %" PRIu64 "\n", sir_sizeof(w->t, type));
    for (k = 0; k < st->nfields; k++) {
      cs_buf_printf(info, "\t.byte %d\n", AB_MEMBER);
      put_strp(w, st->fields[k].name);
      put_type_ref(w, st->fields[k].type);
      cs_buf_printf(info, "\t.long %" PRIu64 "\n", st->fields[k].offset);
    }
    cs_buf_puts(info, "\t.byte 0\n");
  } else if (type->kind == MT_ARRAY) {
    cs_buf_printf(info, "\t.byte %d\n", AB_ARRAY);
    put_type_ref(w, type->elem);
    cs_buf_printf(info, "\t.byte %d\n\t.quad %" PRIu64 "\n\t.byte 0\n", AB_SUBRANGE, type->count);
  } else {
    base_type(w, type);
  }
}

/* DW_OP_bregN and the offset, as an expression with its length. */
static void put_location(Dw *w, const SirDebugVar *v) {
  bool arm = strncmp(w->t->name, "arm64", 5) == 0;
  int reg = v->base == SIR_BASE_FP ? (arm ? 29 : 6) : (arm ? 31 : 7);
  unsigned char sleb[10];
  int64_t x = v->offset;
  int n = 0;
  int i;
  for (;;) {
    unsigned char byte = (unsigned char)(x & 0x7f);
    bool done;
    x = x >= 0 ? x / 128 : -((-x + 127) / 128); /* an arithmetic shift by 7 */
    done = (x == 0 && (byte & 0x40) == 0) || (x == -1 && (byte & 0x40) != 0);
    sleb[n++] = done ? byte : (unsigned char)(byte | 0x80);
    if (done) {
      break;
    }
  }
  cs_buf_printf(&w->x->info, "\t.byte %d, 0x%02x", n + 1, DW_OP_breg0 + reg);
  for (i = 0; i < n; i++) {
    cs_buf_printf(&w->x->info, ", 0x%02x", sleb[i]);
  }
  cs_buf_putc(&w->x->info, '\n');
}

/* The locals of scope k. */
static void put_locals(Dw *w, const SirVarsFunc *f, int k) {
  size_t i;
  for (i = 0; i < f->d.nlocals; i++) {
    const SirLocal *l = f->d.locals[i];
    if (f->d.scope[i] != k || f->vars[i].base == SIR_BASE_NONE) {
      continue;
    }
    cs_buf_printf(&w->x->info, "\t.byte %d\n", l->is_param ? AB_PARAM : AB_VARIABLE);
    put_strp(w, l->name);
    put_type_ref(w, l->type);
    put_location(w, &f->vars[i]);
  }
}

/* A DW_TAG_lexical_block from PREFIXcs_{b}ID_J to PREFIXcs_{e}ID_J, without
 * the end of its children. */
static void put_block(Dw *w, const SirVarsFunc *f, const char *b, const char *e, int j) {
  CsBuf lo = {NULL, 0, 0};
  CsBuf hi = {NULL, 0, 0};
  cs_buf_printf(&hi, "%scs_%s%" PRIu32 "_%d", w->p, e, f->id, j);
  cs_buf_printf(&lo, "%scs_%s%" PRIu32 "_%d", w->p, b, f->id, j);
  cs_buf_printf(&w->x->info, "\t.byte %d\n", AB_BLOCK);
  put_addr(w, lo.data);
  put_diff(w, hi.data, lo.data);
  cs_buf_free(&lo);
  cs_buf_free(&hi);
}

/* The locals of scope k, then its nested scopes. */
static void put_scope(Dw *w, const SirVarsFunc *f, int k) {
  size_t j;
  put_locals(w, f, k);
  for (j = 1; j < f->d.nscopes; j++) {
    if (f->d.parent[j] != k || !f->opened[j]) {
      continue;
    }
    put_block(w, f, "sb", "se", (int)j);
    put_scope(w, f, (int)j);
    cs_buf_puts(&w->x->info, "\t.byte 0\n");
  }
}

/* Each trap's block, out of its scopes: their locals again, nested. */
static void put_traps(Dw *w, const SirVarsFunc *f) {
  CsArena a;
  int *chain;
  size_t j;
  memset(&a, 0, sizeof a);
  chain = cs_arena_new(&a, cs_mul_size(f->d.nscopes, sizeof *chain));
  for (j = 0; j < sir_vars_ntraps(f); j++) {
    size_t len = sir_vars_chain(f, j, chain);
    size_t i;
    for (i = 0; i < len; i++) {
      put_block(w, f, "tb", "te", (int)j);
      put_locals(w, f, chain[i]);
    }
    for (i = 0; i < len; i++) {
      cs_buf_puts(&w->x->info, "\t.byte 0\n");
    }
  }
  cs_arena_free(&a);
}

static void put_function(Dw *w, const SirVarsFunc *f) {
  CsBuf a = {NULL, 0, 0};
  CsBuf b = {NULL, 0, 0};
  cs_buf_printf(&a, "%scs_fe%" PRIu32, w->p, f->id);
  cs_buf_printf(&b, "%scs_fb%" PRIu32, w->p, f->id);
  cs_buf_printf(&w->x->info, "\t.byte %d\n", AB_SUBPROGRAM);
  put_strp(w, f->d.f->name);
  put_addr(w, b.data);
  put_diff(w, a.data, b.data);
  put_scope(w, f, 0);
  put_traps(w, f);
  cs_buf_puts(&w->x->info, "\t.byte 0\n");
  cs_buf_free(&a);
  cs_buf_free(&b);
}

void sir_dwarf_rewrite(CsBuf *out, const char *in, size_t len, const SirModule *m, const SirTarget *t,
                       const char *file) {
  static const char end_fn[] = "/* end function ";
  SirAsmLines lines = {false, false};
  bool in_func = false;
  SirVars v;
  SirDwarfExtra x;
  Dw w;
  size_t i = 0;
  size_t k;
  sir_vars_init(&v, m, t);
  memset(&x, 0, sizeof x);
  sir_dwarf_head(out, t);
  while (i < len) {
    const char *line = in + i;
    const char *nl = memchr(line, '\n', len - i);
    size_t n = nl == NULL ? len - i : (size_t)(nl - line);
    size_t text = n > 0 && line[n - 1] == '\r' ? n - 1 : n;
    i += nl == NULL ? n : n + 1;
    /* a block's label inside a function may follow a directive too */
    if (sir_asm_func_label(&lines, line, text) && !in_func) {
      CsBuf name = {NULL, 0, 0};
      cs_buf_put(&name, line, text - 1);
      sir_vars_begin(&v, name.data);
      cs_buf_free(&name);
      in_func = true;
      cs_buf_put(out, line, n);
      cs_buf_putc(out, '\n');
      if (v.cur != NULL) {
        cs_buf_printf(out, "%scs_fb%" PRIu32 ":\n", v.prefix, v.cur->id);
      }
      continue;
    }
    if (text >= sizeof end_fn && memcmp(line, end_fn, sizeof end_fn - 1) == 0) {
      in_func = false;
      if (v.cur != NULL) {
        uint32_t id = v.cur->id;
        (void)sir_vars_end(&v, out, true);
        cs_buf_printf(out, "%scs_fe%" PRIu32 ":\n", v.prefix, id);
      }
    } else {
      sir_vars_line(&v, out, line, text);
    }
    cs_buf_put(out, line, n);
    if (nl != NULL) {
      cs_buf_putc(out, '\n');
    }
  }
  memset(&w, 0, sizeof w);
  w.t = t;
  w.p = v.prefix;
  w.x = &x;
  put_abbrevs(&x.abbrev);
  for (k = 0; k < v.funcs.len / sizeof(SirVarsFunc *); k++) {
    const SirVarsFunc *f = ((SirVarsFunc **)v.funcs.data)[k];
    if (f != NULL) {
      put_function(&w, f);
    }
  }
  for (k = 0; k < w.types.len / sizeof(SirType *); k++) {
    put_type(&w, k);
  }
  sir_dwarf_tail(out, t, file, &x);
  cs_buf_free(&w.types);
  cs_buf_free(&x.info);
  cs_buf_free(&x.abbrev);
  cs_buf_free(&x.str);
  sir_vars_free(&v);
}
