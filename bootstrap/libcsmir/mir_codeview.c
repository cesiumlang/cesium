/* CodeView line tables for QBE's assembly on COFF targets
 * (cesiumlang/vscode-cesium#47).
 *
 * Windows debuggers read CodeView from a PDB, not DWARF. QBE writes .file and
 * .loc, from which the assembler would make DWARF; this rewrites them into
 * LLVM's CodeView directives (.cv_file, .cv_func_id, .cv_loc,
 * .cv_linetable), from which the devkit's assembler writes .debug$S and the
 * linker the PDB. Each function also gets a procedure record, and the unit
 * an S_COMPILE3, as clang writes them. Source paths are absolute, with
 * backslashes: the linker would make a relative one absolute with forward
 * slashes, which Windows debuggers do not match against a breakpoint's path.
 */
#include "mir.h"

/* CodeView record kinds and the symbol subsection */
#define CV_SIGNATURE_C13 4
#define DEBUG_S_SYMBOLS 0xf1
#define S_END 0x0006
#define S_GPROC32 0x1110
#define S_COMPILE3 0x113c
#define CV_CFL_C 0x00
#define CV_CFL_X64 0xd0

bool mir_codeview_target(const MirTarget *t) {
  return t->format == MIR_COFF;
}

static bool is_sep(char c) {
  return c == '/' || c == '\\';
}

static bool has_drive(const char *p) {
  return ((p[0] >= 'A' && p[0] <= 'Z') || (p[0] >= 'a' && p[0] <= 'z')) && p[1] == ':';
}

static bool is_absolute(const char *p) {
  return is_sep(p[0]) || (has_drive(p) && is_sep(p[2]));
}

/* Whether the path ends in the component "..", after its root. */
static bool ends_in_parent(const CsBuf *o, size_t root) {
  return o->len >= root + 2 && o->data[o->len - 1] == '.' && o->data[o->len - 2] == '.' &&
         (o->len == root + 2 || o->data[o->len - 3] == '\\');
}

void mir_codeview_path(CsBuf *out, const char *file) {
  CsBuf full = {NULL, 0, 0};
  CsBuf o = {NULL, 0, 0};
  const char *p;
  size_t i = 0;
  size_t root;
  if (!is_absolute(file) && cs_current_dir(&full) && full.len != 0 && !is_sep(full.data[full.len - 1])) {
    cs_buf_putc(&full, '\\');
  }
  cs_buf_puts(&full, file);
  p = full.data;
  /* the root: a drive, then one separator; or a UNC prefix */
  if (has_drive(p)) {
    cs_buf_put(&o, p, 2);
    i = 2;
  }
  if (is_sep(p[i])) {
    cs_buf_putc(&o, '\\');
    i++;
    if (o.len == 1 && is_sep(p[i])) {
      cs_buf_putc(&o, '\\');
      i++;
    }
  }
  root = o.len;
  /* the components, without . and with .. taken back */
  while (p[i] != '\0') {
    size_t start = i;
    size_t len;
    while (p[i] != '\0' && !is_sep(p[i])) {
      i++;
    }
    len = i - start;
    if (len == 2 && p[start] == '.' && p[start + 1] == '.' && o.len > root && !ends_in_parent(&o, root)) {
      while (o.len > root && o.data[o.len - 1] != '\\') {
        o.len--;
      }
      if (o.len > root) {
        o.len--;
      }
      o.data[o.len] = '\0';
    } else if (len != 0 && !(len == 1 && p[start] == '.')) {
      if (o.len > root) {
        cs_buf_putc(&o, '\\');
      }
      cs_buf_put(&o, p + start, len);
    }
    if (p[i] != '\0') {
      i++;
    }
  }
  cs_buf_put(out, o.data == NULL ? "" : o.data, o.len);
  cs_buf_free(&o);
  cs_buf_free(&full);
}

static bool starts_with(const char *s, size_t len, const char *prefix) {
  size_t n = strlen(prefix);
  return len >= n && memcmp(s, prefix, n) == 0;
}

/* The procedure record of a function, named by its symbol as QBE writes it:
 * COFF symbols have no prefix, and a quoted one stays quoted. */
static void put_procedure(CsBuf *out, uint32_t id, const char *name) {
  cs_buf_printf(out, "\t.short .Lcs_cv_p%" PRIu32 "_end - .Lcs_cv_p%" PRIu32 "\n", id, id);
  cs_buf_printf(out, ".Lcs_cv_p%" PRIu32 ":\n\t.short 0x%04x\n", id, S_GPROC32);
  /* parent, end, next: the linker fills them in */
  cs_buf_puts(out, "\t.long 0, 0, 0\n");
  cs_buf_printf(out, "\t.long .Lcs_cv_end%" PRIu32 " - %s\n", id, name);
  /* debug start, debug end, type */
  cs_buf_puts(out, "\t.long 0, 0, 0\n");
  cs_buf_printf(out, "\t.secrel32 %s\n\t.secidx %s\n\t.byte 0\n\t.asciz ", name, name);
  if (name[0] == '"') {
    cs_buf_puts(out, name);
  } else {
    mir_asm_string(out, name);
  }
  cs_buf_printf(out, "\n\t.p2align 2\n.Lcs_cv_p%" PRIu32 "_end:\n", id);
  cs_buf_printf(out, "\t.short 2\n\t.short 0x%04x\n", S_END);
}

/* The .debug$S section: one symbol subsection, then the line tables and the
 * file tables the assembler makes. names holds a line per function. */
static void put_tail(CsBuf *out, const CsBuf *names) {
  const char *p = names->data;
  uint32_t id;
  cs_buf_puts(out, "/* CodeView (libcsmir) */\n.section .debug$S,\"dr\"\n\t.p2align 2\n");
  cs_buf_printf(out, "\t.long %d\n\t.long 0x%02x\n", CV_SIGNATURE_C13, DEBUG_S_SYMBOLS);
  cs_buf_puts(out, "\t.long .Lcs_cv_syms_end - .Lcs_cv_syms\n.Lcs_cv_syms:\n");
  cs_buf_puts(out, "\t.short .Lcs_cv_unit_end - .Lcs_cv_unit\n.Lcs_cv_unit:\n");
  /* flags and language; machine; front end and back end versions */
  cs_buf_printf(out, "\t.short 0x%04x\n\t.long 0x%02x\n\t.short 0x%02x\n", S_COMPILE3, CV_CFL_C, CV_CFL_X64);
  cs_buf_puts(out, "\t.short 0, 0, 0, 0\n\t.short 0, 0, 0, 0\n\t.asciz \"libcsmir\"\n");
  cs_buf_puts(out, "\t.p2align 2\n.Lcs_cv_unit_end:\n");
  for (id = 0; *p != '\0'; id++) {
    const char *nl = strchr(p, '\n');
    CsBuf name = {NULL, 0, 0};
    cs_buf_put(&name, p, (size_t)(nl - p));
    put_procedure(out, id, name.data);
    cs_buf_free(&name);
    p = nl + 1;
  }
  cs_buf_puts(out, ".Lcs_cv_syms_end:\n\t.p2align 2\n");
  p = names->data;
  for (id = 0; *p != '\0'; id++) {
    const char *nl = strchr(p, '\n');
    cs_buf_printf(out, "\t.cv_linetable %" PRIu32 ", %.*s, .Lcs_cv_end%" PRIu32 "\n", id, (int)(nl - p), p, id);
    p = nl + 1;
  }
  cs_buf_puts(out, "\t.cv_filechecksums\n\t.cv_stringtable\n");
}

void mir_codeview_rewrite(CsBuf *out, const char *in, size_t len) {
  static const char end_fn[] = "/* end function ";
  CsBuf names = {NULL, 0, 0};
  CsBuf loc = {NULL, 0, 0};  /* the last .loc, not yet written */
  CsBuf body = {NULL, 0, 0}; /* a prologue, held */
  CsBuf *dst = out;
  bool held = false;
  bool in_text = false;
  bool was_directive = false;
  bool has_id = false;
  uint32_t next_id = 0;
  size_t i = 0;
  cs_buf_put(&names, "", 0);
  while (i < len) {
    const char *line = in + i;
    const char *nl = memchr(line, '\n', len - i);
    size_t n = nl == NULL ? len - i : (size_t)(nl - line);
    size_t text = n > 0 && line[n - 1] == '\r' ? n - 1 : n; /* without the line ending */
    i += nl == NULL ? n : n + 1;
    bool directive = text > 0 && line[0] == '.';
    bool after_directive = was_directive;
    was_directive = directive;
    if (starts_with(line, text, ".file ")) {
      cs_buf_puts(out, ".cv_file ");
      cs_buf_put(out, line + 6, n - 6);
      cs_buf_putc(out, '\n');
      continue;
    }
    if (directive) {
      in_text = starts_with(line, text, ".text") ||
                (in_text && !starts_with(line, text, ".data") && !starts_with(line, text, ".bss") &&
                 !starts_with(line, text, ".section"));
    }
    /* a function's label: in the text, right after its directives; its
     * prologue is held until the function's first line is known */
    if (in_text && after_directive && !held && !has_id && text > 1 && line[text - 1] == ':' && line[0] != '\t' &&
        line[0] != '/') {
      cs_buf_put(out, line, n);
      cs_buf_putc(out, '\n');
      body.len = 0;
      held = true;
      continue;
    }
    if (starts_with(line, text, "\t.loc ")) {
      if (!has_id) {
        cs_buf_printf(out, "\t.cv_func_id %" PRIu32 "\n", next_id);
        has_id = true;
        /* the entry gets the line of the first statement, as debuggers
         * stop there when stepping in */
        if (held) {
          cs_buf_printf(out, "\t.cv_loc %" PRIu32 " ", next_id);
          cs_buf_put(out, line + 6, n - 6);
          cs_buf_putc(out, '\n');
          cs_buf_put(out, body.data == NULL ? "" : body.data, body.len);
          held = false;
        }
      }
      /* of several in a row, the last is the line of the code after them */
      loc.len = 0;
      cs_buf_printf(&loc, "\t.cv_loc %" PRIu32 " ", next_id);
      cs_buf_put(&loc, line + 6, n - 6);
      cs_buf_putc(&loc, '\n');
      continue;
    }
    dst = held ? &body : out;
    if (starts_with(line, text, end_fn) && text >= sizeof end_fn + 3 && memcmp(line + text - 3, " */", 3) == 0) {
      loc.len = 0;
      if (held) {
        cs_buf_put(out, body.data == NULL ? "" : body.data, body.len);
        held = false;
        dst = out;
      }
      if (has_id) {
        cs_buf_printf(out, ".Lcs_cv_end%" PRIu32 ":\n", next_id);
        /* the name, between the prefix and the comment's end */
        cs_buf_put(&names, line + sizeof end_fn - 1, text - (sizeof end_fn - 1) - 3);
        cs_buf_putc(&names, '\n');
        has_id = false;
        next_id++;
      }
    } else if (loc.len != 0) {
      cs_buf_put(dst, loc.data, loc.len);
      loc.len = 0;
    }
    cs_buf_put(dst, line, n);
    if (nl != NULL) {
      cs_buf_putc(dst, '\n');
    }
  }
  if (held) {
    cs_buf_put(out, body.data == NULL ? "" : body.data, body.len);
  }
  if (next_id != 0) {
    put_tail(out, &names);
  }
  cs_buf_free(&body);
  cs_buf_free(&loc);
  cs_buf_free(&names);
}
