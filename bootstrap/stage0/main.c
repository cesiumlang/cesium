/* cesium0: the stage 0 compiler. It compiles S0 (cesiumlang/cesium#9), or
 * core MIR text, to assembly through libcsmir and QBE.
 *
 *   cesium0 [options] -o <out> <file.csm>...
 *   cesium0 [options] -o <out> --mir <file.mir>
 *
 *   -o <out>        the output file (required)
 *   --target <t>    a QBE target name or an <arch>-<os> triple; default: this machine
 *   --emit-mir      write the core MIR text to <out> and stop
 *   --emit-qbe      write the QBE IL to <out> and stop
 *   --mir <file>    read core MIR text instead of S0 source
 *   -O <mode>       the build mode, as Zig names them: debug (the default),
 *                   safe, fast or small
 *   -fdebug-variables, -fno-debug-variables
 *                   keep variables in memory and describe them for debuggers,
 *                   or not; the default of a debug build only
 *                   (cesiumlang/cesium#143)
 *
 * Without --emit-*, <out> is assembly text and "<out>.ssa" is the QBE IL it
 * was made from. Several .csm files are compiled as one unit.
 *
 * Exit status: 0 on success, 1 on an error in the input, 2 on a usage error.
 */
#include "csmir.h"
#include "csmir_internal.h"
#include "s0.h"

static int usage(const char *msg) {
  if (msg != NULL) {
    fprintf(stderr, "cesium0: %s\n", msg);
  }
  fputs("usage: cesium0 [--target <t>] [-O <mode>] [-f[no-]debug-variables] [--emit-mir | --emit-qbe]\n"
        "               -o <out> (<file.csm>... | --mir <file.mir>)\n",
        stderr);
  return 2;
}

/* The options that take a value. */
static bool takes_value(const char *a) {
  return strcmp(a, "-o") == 0 || strcmp(a, "--target") == 0 || strcmp(a, "--mir") == 0 || strcmp(a, "-O") == 0;
}

int main(int argc, char *argv[]) {
  const char *out = NULL;
  const char *target_name = NULL;
  const char *mir_path = NULL;
  bool emit_mir = false;
  bool emit_qbe = false;
  int mode = CS_OPT_DEBUG;
  int variables = -1; /* -1: the mode's default */
  const MirTarget *target;
  MirModule *m;
  CsArena arena = {NULL};
  S0Diag diag = {0};
  int nfiles = 0;
  int i;

  for (i = 1; i < argc; i++) {
    const char *a = argv[i];
    if (takes_value(a)) {
      if (i + 1 >= argc) {
        return usage("missing value after an option");
      }
      if (a[1] == 'o') {
        out = argv[++i];
      } else if (a[1] == 'O') {
        if (!csmir_opt_mode(argv[++i], &mode)) {
          return usage("unknown build mode (-O debug, safe, fast or small)");
        }
      } else if (a[2] == 't') {
        target_name = argv[++i];
      } else {
        mir_path = argv[++i];
      }
    } else if (strcmp(a, "-fdebug-variables") == 0) {
      variables = 1;
    } else if (strcmp(a, "-fno-debug-variables") == 0) {
      variables = 0;
    } else if (strcmp(a, "--emit-mir") == 0) {
      emit_mir = true;
    } else if (strcmp(a, "--emit-qbe") == 0) {
      emit_qbe = true;
    } else if (a[0] == '-') {
      return usage("unknown option");
    } else {
      nfiles++;
    }
  }
  if (out == NULL) {
    return usage("no output file (-o)");
  }
  if ((mir_path == NULL) == (nfiles == 0)) {
    return usage("give S0 source files or --mir <file>, not both");
  }
  if (variables < 0) {
    variables = mode == CS_OPT_DEBUG;
  }

  cs_backend_init();
  target = csmir_target(target_name);
  if (target == NULL) {
    fprintf(stderr, "cesium0: %s\n", cs_last_error());
    return 1;
  }

  if (mir_path != NULL) {
    char *text;
    size_t len;
    if (!cs_read_file(mir_path, &text, &len)) {
      fprintf(stderr, "cesium0: cannot read %s\n", mir_path);
      return 1;
    }
    m = mir_parse(text, len);
    free(text);
    if (m->failed) {
      fprintf(stderr, "%s: error[%s]: %s\n", mir_path, m->err_code, m->err);
      return 1;
    }
  } else {
    S0Decl *head = NULL;
    S0Decl *tail = NULL;
    for (i = 1; i < argc; i++) {
      const char *a = argv[i];
      char *text;
      size_t len;
      if (takes_value(a)) {
        i++;
        continue;
      }
      if (a[0] == '-') {
        continue;
      }
      if (!cs_read_file(a, &text, &len)) {
        fprintf(stderr, "cesium0: cannot read %s\n", a);
        return 1;
      }
      s0_parse_file(&arena, &diag, a, text, len, &head, &tail);
      free(text);
    }
    if (diag.errors != 0) {
      return 1;
    }
    m = s0_lower(&arena, &diag, head);
    if (m == NULL) {
      return 1;
    }
  }

  if (!mir_verify(m)) {
    /* A few errors in S0 source are left to the verifier: a function that
     * does not return on every path, a struct that contains itself, and a
     * name that collides with the runtime's. */
    fprintf(stderr, "%s: error[%s]: %s\n", mir_path != NULL ? mir_path : "cesium0", m->err_code, m->err);
    return 1;
  }

  if (emit_mir || emit_qbe) {
    CsBuf b = {NULL, 0, 0};
    if (emit_mir) {
      mir_print(&b, m);
    } else {
      mir_lower_qbe(&b, m, target, variables != 0);
    }
    if (!cs_write_file(out, b.data == NULL ? "" : b.data, b.len)) {
      fprintf(stderr, "cesium0: cannot write %s\n", out);
      return 1;
    }
    cs_buf_free(&b);
    mir_module_free(m);
    cs_arena_free(&arena);
    return 0;
  }

  /* does not return on success: QBE ends the process */
  if (csmir_emit_qbe(m, target, out, variables != 0) != 0) {
    fprintf(stderr, "cesium0: %s\n", cs_last_error());
  }
  return 1;
}
