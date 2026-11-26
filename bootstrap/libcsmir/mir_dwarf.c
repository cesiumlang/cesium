/* A DWARF compile unit for the line table of QBE's assembly
 * (cesiumlang/cesium#78).
 *
 * QBE writes .file and .loc, from which the assembler makes .debug_line, but
 * no .debug_info; debuggers use a line table only through a compile unit
 * that covers the code. This adds the smallest such unit: DWARF 4, the
 * version of the line table the devkit's assembler writes, with one
 * DW_TAG_compile_unit and no children. COFF gets none: Windows debuggers
 * read CodeView, not DWARF.
 */
#include "mir.h"

/* DWARF 4, section 7.5 */
#define DW_TAG_compile_unit 0x11
#define DW_AT_name 0x03
#define DW_AT_stmt_list 0x10
#define DW_AT_low_pc 0x11
#define DW_AT_high_pc 0x12
#define DW_AT_language 0x13
#define DW_AT_comp_dir 0x1b
#define DW_AT_producer 0x25
#define DW_FORM_addr 0x01
#define DW_FORM_data2 0x05
#define DW_FORM_data4 0x06
#define DW_FORM_strp 0x0e
#define DW_FORM_sec_offset 0x17
#define DW_LANG_C99 0x0c

void mir_asm_string(CsBuf *out, const char *s) {
  const unsigned char *c;
  cs_buf_putc(out, '"');
  for (c = (const unsigned char *)s; *c != '\0'; c++) {
    if (*c == '"' || *c == '\\') {
      cs_buf_printf(out, "\\%c", *c);
    } else if (*c >= 0x20 && *c <= 0x7e) {
      cs_buf_putc(out, (char)*c);
    } else {
      cs_buf_printf(out, "\\%03o", (unsigned)*c);
    }
  }
  cs_buf_putc(out, '"');
}

bool mir_dwarf_target(const MirTarget *t) {
  return t->format == MIR_ELF || t->format == MIR_MACHO;
}

/* The prefix of labels the assembler keeps out of the symbol table. */
static const char *local_prefix(const MirTarget *t) {
  return t->format == MIR_MACHO ? "L" : ".L";
}

void mir_dwarf_head(CsBuf *out, const MirTarget *t) {
  cs_buf_printf(out, ".text\n%scs_text_start:\n", local_prefix(t));
}

/* Switches to a debug section; flags are ELF's. */
static void section(CsBuf *out, const MirTarget *t, const char *name, const char *flags) {
  if (t->format == MIR_MACHO) {
    cs_buf_printf(out, ".section __DWARF,__%s,regular,debug\n", name);
  } else {
    cs_buf_printf(out, ".section .%s,%s\n", name, flags);
  }
}

/* The offset of a label in its section. ELF relocates it, as GCC writes it;
 * Mach-O debug sections stay in the object, where this unit's come first,
 * so it is the label's distance from the section's start (none: 0). .set
 * makes the assembler compute it, with no relocation. */
static void sec_offset(CsBuf *out, const MirTarget *t, const char *label, const char *start) {
  const char *p = local_prefix(t);
  if (t->format != MIR_MACHO) {
    cs_buf_printf(out, "\t.long %scs_%s\n", p, label);
  } else if (start == NULL) {
    cs_buf_puts(out, "\t.long 0\n");
  } else {
    cs_buf_printf(out, "\t.set %scs_%s_off, %scs_%s - %scs_%s\n", p, label, p, label, p, start);
    cs_buf_printf(out, "\t.long %scs_%s_off\n", p, label);
  }
}

void mir_dwarf_tail(CsBuf *out, const MirTarget *t, const char *file) {
  const char *p = local_prefix(t);
  cs_buf_printf(out, "/* DWARF compile unit (libcsmir) */\n.text\n%scs_text_end:\n", p);
  /* the assembler puts its line table after the label */
  if (t->format != MIR_MACHO) {
    cs_buf_printf(out, ".section .debug_line,\"\",@progbits\n%scs_line:\n", p);
  }
  /* strings as DW_FORM_strp: Zig's Mach-O linker misreads DW_FORM_string */
  section(out, t, "debug_str", "\"MS\",@progbits,1");
  cs_buf_printf(out, "%scs_producer:\n\t.asciz \"libcsmir\"\n%scs_name:\n\t.asciz ", p, p);
  mir_asm_string(out, file);
  cs_buf_printf(out, "\n%scs_dir:\n\t.asciz \".\"\n", p);
  section(out, t, "debug_abbrev", "\"\",@progbits");
  /* abbreviation 1: the compile unit, no children, its attributes */
  cs_buf_printf(out, "%scs_abbrev:\n\t.byte 1, 0x%02x, 0\n", p, DW_TAG_compile_unit);
  cs_buf_printf(out, "\t.byte 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x\n", DW_AT_producer,
                DW_FORM_strp, DW_AT_language, DW_FORM_data2, DW_AT_name, DW_FORM_strp, DW_AT_comp_dir, DW_FORM_strp);
  cs_buf_printf(out, "\t.byte 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0, 0\n", DW_AT_stmt_list,
                DW_FORM_sec_offset, DW_AT_low_pc, DW_FORM_addr, DW_AT_high_pc, DW_FORM_data4);
  cs_buf_puts(out, "\t.byte 0\n");
  section(out, t, "debug_info", "\"\",@progbits");
  cs_buf_printf(out, "\t.long %scs_info_end - %scs_info_start\n%scs_info_start:\n\t.short 4\n", p, p, p);
  sec_offset(out, t, "abbrev", NULL);
  cs_buf_printf(out, "\t.byte %d\n\t.byte 1\n", (int)t->ptr_size);
  sec_offset(out, t, "producer", "producer");
  cs_buf_printf(out, "\t.short 0x%02x\n", DW_LANG_C99);
  sec_offset(out, t, "name", "producer");
  sec_offset(out, t, "dir", "producer");
  sec_offset(out, t, "line", NULL);
  cs_buf_printf(out, "\t%s %scs_text_start\n", t->ptr_size == 8 ? ".quad" : ".long", p);
  cs_buf_printf(out, "\t.set %scs_text_size, %scs_text_end - %scs_text_start\n", p, p, p);
  cs_buf_printf(out, "\t.long %scs_text_size\n%scs_info_end:\n", p, p);
}
