/* The coarse API of libcsir and the in-process QBE driver. */
#include "sir.h"
#include "sir_internal.h"

/* The pinned QBE's own main(), renamed by the devkit build (libqbe.a). */
int qbe_main(int argc, char *argv[]);

static char last_error[600];
static const char *last_code = "";

static void set_error(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  cs_vsnprintf(last_error, sizeof last_error, fmt, ap);
  va_end(ap);
  last_code = "";
}

/* An error in the SIR, with its code. */
static void set_sir_error(const SirModule *m) {
  set_error("SIR: %s", m->err);
  last_code = m->err_code != NULL ? m->err_code : "";
}

const char *cs_last_error(void) {
  return last_error;
}

const char *cs_last_error_code(void) {
  return last_code;
}

int cs_backend_init(void) {
  last_error[0] = '\0';
  return 0;
}

static bool starts_with(const char *s, const char *prefix) {
  return strncmp(s, prefix, strlen(prefix)) == 0;
}

const SirTarget *csir_target(const char *triple) {
  static const struct {
    const char *prefix;
    const char *qbe;
  } map[] = {
      {"x86_64-linux", "amd64_sysv"},  {"x86_64-windows", "amd64_win"}, {"x86_64-macos", "amd64_apple"},
      {"aarch64-linux", "arm64"},      {"aarch64-macos", "arm64_apple"}, {"riscv64-linux", "rv64"},
  };
  const SirTarget *t;
  size_t i;
  if (triple == NULL || triple[0] == '\0') {
    t = sir_target_host();
    if (t == NULL) {
      set_error("QBE has no target for this machine; name one");
    }
    return t;
  }
  t = sir_target_by_name(triple);
  if (t != NULL) {
    return t;
  }
  for (i = 0; i < sizeof map / sizeof map[0]; i++) {
    if (starts_with(triple, map[i].prefix)) {
      return sir_target_by_name(map[i].qbe);
    }
  }
  set_error("the QBE backend has no target for '%s'", triple);
  return NULL;
}

/* What goes after QBE's assembly; QBE ends the process without closing its
 * output, so it is written to the same stream at exit. */
static CsBuf dwarf_tail = {NULL, 0, 0};

static void write_dwarf_tail(void) {
  fwrite(dwarf_tail.data, 1, dwarf_tail.len, stdout);
}

/* Makes QBE write to stdout, opened on out_path, between a DWARF head and
 * tail. Returns false with the error set. */
static bool wrap_in_dwarf(const SirTarget *t, const char *out_path, const char *file) {
  CsBuf head = {NULL, 0, 0};
  /* "-" is stdout for QBE too */
  if (strcmp(out_path, "-") != 0 && freopen(out_path, "w", stdout) == NULL) {
    set_error("cannot write %s", out_path);
    return false;
  }
  sir_dwarf_head(&head, t);
  fwrite(head.data, 1, head.len, stdout);
  cs_buf_free(&head);
  sir_dwarf_tail(&dwarf_tail, t, file, NULL);
  if (atexit(write_dwarf_tail) != 0) {
    set_error("cannot register the DWARF tail of %s", out_path);
    return false;
  }
  return true;
}

/* COFF, and ELF and Mach-O with variables: QBE writes to a file beside the
 * output, which at exit is rewritten into the output, with CodeView
 * (sir_codeview.c) or with a DWARF compile unit and its variables
 * (sir_dwarf_vars.c). The module lives until then, for its variables, or is
 * NULL without them. */
static CsBuf qbe_path = {NULL, 0, 0};
static CsBuf rw_path = {NULL, 0, 0};
static const SirModule *rw_module = NULL;
static const SirTarget *rw_target = NULL;
static const char *rw_file = NULL;

static void write_rewrite(void) {
  char *data = NULL;
  size_t len = 0;
  CsBuf out = {NULL, 0, 0};
  bool ok = fclose(stdout) == 0 && cs_read_file(qbe_path.data, &data, &len);
  if (ok) {
    if (sir_codeview_target(rw_target)) {
      sir_codeview_rewrite(&out, data, len, rw_module, rw_target);
    } else {
      sir_dwarf_rewrite(&out, data, len, rw_module, rw_target, rw_file);
    }
    ok = cs_write_file(rw_path.data, out.data == NULL ? "" : out.data, out.len);
  }
  remove(qbe_path.data);
  free(data);
  cs_buf_free(&out);
  if (!ok) {
    fprintf(stderr, "cannot write %s\n", rw_path.data);
    /* exit() may not be called again from a handler it runs */
    _Exit(1);
  }
}

/* Makes QBE write to stdout, opened on a file beside out_path, which is
 * rewritten into out_path at exit. Returns false with the error set. */
static bool wrap_in_rewrite(const SirTarget *t, const char *out_path, const char *file) {
  rw_target = t;
  rw_file = file;
  cs_buf_puts(&rw_path, out_path);
  cs_buf_printf(&qbe_path, "%s.qbe.s", out_path);
  if (freopen(qbe_path.data, "wb", stdout) == NULL) {
    set_error("cannot write %s", qbe_path.data);
    return false;
  }
  if (atexit(write_rewrite) != 0) {
    set_error("cannot register the rewrite of %s", out_path);
    return false;
  }
  return true;
}

bool csir_opt_mode(const char *name, int *mode) {
  static const char *const names[] = {"debug", "safe", "fast", "small"};
  int i;
  for (i = 0; i < 4; i++) {
    if (strcmp(name, names[i]) == 0) {
      *mode = i;
      return true;
    }
  }
  return false;
}

int csir_emit_qbe(SirModule *m, const SirTarget *t, const char *out_path, bool variables) {
  CsBuf il = {NULL, 0, 0};
  CsBuf il_path = {NULL, 0, 0};
  char *argv[7];
  char arg0[] = "qbe";
  char arg_t[] = "-t";
  char arg_o[] = "-o";
  char *target;
  char *out;
  const char *file;
  bool rewrite;
  int argc = 6;
  if (!sir_verify(m)) {
    set_sir_error(m);
    return 1;
  }
  file = sir_lower_qbe(&il, m, t, variables);
  if (variables) {
    rw_module = m;
  }
  cs_buf_printf(&il_path, "%s.ssa", out_path);
  if (!cs_write_file(il_path.data, il.data == NULL ? "" : il.data, il.len)) {
    set_error("cannot write %s", il_path.data);
    cs_buf_free(&il);
    cs_buf_free(&il_path);
    return 1;
  }
  cs_buf_free(&il);
  /* QBE takes its arguments as a program does, and ends the process */
  target = cs_arena_str(&m->arena, t->name);
  out = cs_arena_str(&m->arena, out_path);
  argv[0] = arg0;
  argv[1] = arg_t;
  argv[2] = target;
  argv[3] = arg_o;
  argv[4] = out;
  argv[5] = il_path.data;
  argv[6] = NULL;
  fflush(stdout);
  fflush(stderr);
  /* "-" (stdout) gets no CodeView and no variables: there is no file to
   * rewrite; DWARF without variables is streamed around QBE's output */
  rewrite = strcmp(out_path, "-") != 0 && (sir_codeview_target(t) || (variables && sir_dwarf_target(t)));
  if (file != NULL && (rewrite || sir_dwarf_target(t))) {
    if (!(rewrite ? wrap_in_rewrite(t, out_path, file) : wrap_in_dwarf(t, out_path, file))) {
      cs_buf_free(&il_path);
      return 1;
    }
    /* no -o: QBE writes to stdout */
    argv[3] = il_path.data;
    argv[4] = NULL;
    argc = 4;
  }
  return qbe_main(argc, argv);
}

int cs_compile_sir(const char *sir, size_t len, int backend, const char *triple, const char *cpu,
                   const char *features, int opt_level, const char *out_path) {
  const SirTarget *t;
  SirModule *m;
  int rc;
  CS_UNUSED(cpu);
  CS_UNUSED(features);
  if (backend != CS_BACKEND_QBE) {
    set_error("only the QBE backend exists so far");
    return 1;
  }
  if ((opt_level & ~CS_OPT_VARIABLES) < CS_OPT_DEBUG || (opt_level & ~CS_OPT_VARIABLES) > CS_OPT_SMALL) {
    set_error("unknown build mode %d", opt_level);
    return 1;
  }
  t = csir_target(triple);
  if (t == NULL) {
    return 1;
  }
  m = sir_parse(sir, len);
  if (m->failed) {
    set_sir_error(m);
    sir_module_free(m);
    return 1;
  }
  rc = csir_emit_qbe(m, t, out_path, (opt_level & CS_OPT_VARIABLES) != 0);
  sir_module_free(m);
  return rc;
}
