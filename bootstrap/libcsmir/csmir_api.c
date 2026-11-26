/* The coarse API of libcsmir and the in-process QBE driver. */
#include "csmir.h"
#include "csmir_internal.h"

/* The pinned QBE's own main(), renamed by the devkit build (libqbe.a). */
int qbe_main(int argc, char *argv[]);

static char last_error[600];

static void set_error(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  cs_vsnprintf(last_error, sizeof last_error, fmt, ap);
  va_end(ap);
}

const char *cs_last_error(void) {
  return last_error;
}

int cs_backend_init(void) {
  last_error[0] = '\0';
  return 0;
}

static bool starts_with(const char *s, const char *prefix) {
  return strncmp(s, prefix, strlen(prefix)) == 0;
}

const MirTarget *csmir_target(const char *triple) {
  static const struct {
    const char *prefix;
    const char *qbe;
  } map[] = {
      {"x86_64-linux", "amd64_sysv"},  {"x86_64-windows", "amd64_win"}, {"x86_64-macos", "amd64_apple"},
      {"aarch64-linux", "arm64"},      {"aarch64-macos", "arm64_apple"}, {"riscv64-linux", "rv64"},
  };
  const MirTarget *t;
  size_t i;
  if (triple == NULL || triple[0] == '\0') {
    t = mir_target_host();
    if (t == NULL) {
      set_error("QBE has no target for this machine; name one");
    }
    return t;
  }
  t = mir_target_by_name(triple);
  if (t != NULL) {
    return t;
  }
  for (i = 0; i < sizeof map / sizeof map[0]; i++) {
    if (starts_with(triple, map[i].prefix)) {
      return mir_target_by_name(map[i].qbe);
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
static bool wrap_in_dwarf(const MirTarget *t, const char *out_path, const char *file) {
  CsBuf head = {NULL, 0, 0};
  /* "-" is stdout for QBE too */
  if (strcmp(out_path, "-") != 0 && freopen(out_path, "w", stdout) == NULL) {
    set_error("cannot write %s", out_path);
    return false;
  }
  mir_dwarf_head(&head, t);
  fwrite(head.data, 1, head.len, stdout);
  cs_buf_free(&head);
  mir_dwarf_tail(&dwarf_tail, t, file);
  if (atexit(write_dwarf_tail) != 0) {
    set_error("cannot register the DWARF tail of %s", out_path);
    return false;
  }
  return true;
}

int csmir_emit_qbe(MirModule *m, const MirTarget *t, const char *out_path) {
  CsBuf il = {NULL, 0, 0};
  CsBuf il_path = {NULL, 0, 0};
  char *argv[7];
  char arg0[] = "qbe";
  char arg_t[] = "-t";
  char arg_o[] = "-o";
  char *target;
  char *out;
  const char *file;
  int argc = 6;
  if (!mir_verify(m)) {
    set_error("MIR: %s", m->err);
    return 1;
  }
  file = mir_lower_qbe(&il, m, t);
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
  if (file != NULL && mir_dwarf_target(t)) {
    if (!wrap_in_dwarf(t, out_path, file)) {
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

int cs_compile_mir(const char *mir, size_t len, int backend, const char *triple, const char *cpu,
                   const char *features, int opt_level, const char *out_path) {
  const MirTarget *t;
  MirModule *m;
  int rc;
  CS_UNUSED(cpu);
  CS_UNUSED(features);
  CS_UNUSED(opt_level);
  if (backend != CS_BACKEND_QBE) {
    set_error("only the QBE backend exists so far");
    return 1;
  }
  t = csmir_target(triple);
  if (t == NULL) {
    return 1;
  }
  m = mir_parse(mir, len);
  if (m->failed) {
    set_error("MIR: %s", m->err);
    mir_module_free(m);
    return 1;
  }
  rc = csmir_emit_qbe(m, t, out_path);
  mir_module_free(m);
  return rc;
}
