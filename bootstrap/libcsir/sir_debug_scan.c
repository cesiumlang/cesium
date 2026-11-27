/* Reads the markers of sir_debug.c from QBE's assembly, a line at a time
 * (cesiumlang/cesium#143): where each local's slot is, from the instruction
 * that takes its address before the store to its marker, and where each
 * scope's markers are. QBE writes x86-64 as
 *
 *     leaq 28(%rsp), %rax
 *     movq %rax, .Lcs_v0_1(%rip)
 *
 * and AArch64 as
 *
 *     adrp x3, .Lcs_v0_1          (@page and @pageoff on Apple)
 *     add x3, x3, #:lo12:.Lcs_v0_1
 *     add x2, x29, #40            (or mov x16, #N, then add x2, x29, x16)
 *     str x2, [x3]
 */
#include "sir_core.h"

/* An instruction's mnemonic and operands, split at commas outside brackets
 * and parentheses. */
typedef struct Insn {
  const char *op;
  size_t op_len;
  const char *arg[4];
  size_t arg_len[4];
  int nargs;
} Insn;

static bool is_space(char c) {
  return c == ' ' || c == '\t';
}

static void split(Insn *in, const char *s, size_t len) {
  size_t i = 0;
  memset(in, 0, sizeof *in);
  while (i < len && is_space(s[i])) {
    i++;
  }
  in->op = s + i;
  while (i < len && !is_space(s[i])) {
    i++;
  }
  in->op_len = (size_t)(s + i - in->op);
  while (i < len && in->nargs < 4) {
    size_t start;
    size_t end;
    int depth = 0;
    while (i < len && is_space(s[i])) {
      i++;
    }
    start = i;
    while (i < len && (depth > 0 || s[i] != ',')) {
      depth += s[i] == '(' || s[i] == '[' ? 1 : s[i] == ')' || s[i] == ']' ? -1 : 0;
      i++;
    }
    end = i;
    while (end > start && is_space(s[end - 1])) {
      end--;
    }
    if (end > start) {
      in->arg[in->nargs] = s + start;
      in->arg_len[in->nargs] = end - start;
      in->nargs++;
    }
    if (i < len) {
      i++;
    }
  }
}

static bool op_is(const Insn *in, const char *op) {
  return in->op_len == strlen(op) && memcmp(in->op, op, in->op_len) == 0;
}

static bool arg_is(const Insn *in, int k, const char *s) {
  return k < in->nargs && in->arg_len[k] == strlen(s) && memcmp(in->arg[k], s, in->arg_len[k]) == 0;
}

/* A signed decimal at s, its length in *used. */
static bool parse_int(const char *s, size_t len, int64_t *v, size_t *used) {
  size_t i = 0;
  bool neg = false;
  int64_t n = 0;
  if (i < len && (s[i] == '-' || s[i] == '+')) {
    neg = s[i] == '-';
    i++;
  }
  if (i >= len || s[i] < '0' || s[i] > '9') {
    return false;
  }
  for (; i < len && s[i] >= '0' && s[i] <= '9'; i++) {
    n = n * 10 + (s[i] - '0');
  }
  *v = neg ? -n : n;
  *used = i;
  return true;
}

/* The register an operand names: an index into the tables, or -1. AArch64
 * x and w registers by number, then x86-64's 64-bit registers. */
static int reg_of(const char *s, size_t len) {
  static const char *const x86[] = {"%rax", "%rbx", "%rcx", "%rdx", "%rsi", "%rdi", "%rbp", "%rsp",
                                    "%r8",  "%r9",  "%r10", "%r11", "%r12", "%r13", "%r14", "%r15"};
  int64_t n;
  size_t used;
  size_t i;
  if (len >= 2 && (s[0] == 'x' || s[0] == 'w') && parse_int(s + 1, len - 1, &n, &used) && used == len - 1 && n < 32) {
    return (int)n;
  }
  for (i = 0; i < sizeof x86 / sizeof x86[0]; i++) {
    if (len == strlen(x86[i]) && memcmp(s, x86[i], len) == 0) {
      return 32 + (int)i;
    }
  }
  return -1;
}

static bool starts_with(const char *s, size_t len, const char *prefix) {
  size_t n = strlen(prefix);
  return len >= n && memcmp(s, prefix, n) == 0;
}

bool sir_asm_func_label(SirAsmLines *s, const char *line, size_t len) {
  bool directive = len > 0 && line[0] == '.';
  bool after_directive = s->was_directive;
  s->was_directive = directive;
  if (directive) {
    s->in_text = starts_with(line, len, ".text") ||
                 (s->in_text && !starts_with(line, len, ".data") && !starts_with(line, len, ".bss") &&
                  !starts_with(line, len, ".section"));
  }
  return s->in_text && after_directive && len > 1 && line[len - 1] == ':' && line[0] != '\t' && line[0] != '/';
}

void sir_debug_scan_start(SirDebugScan *s, SirDebugVar *vars, size_t nlocals) {
  size_t i;
  memset(s, 0, sizeof *s);
  s->vars = vars;
  s->nlocals = nlocals;
  for (i = 0; i < nlocals; i++) {
    vars[i].base = SIR_BASE_NONE;
    vars[i].offset = 0;
  }
  for (i = 0; i < SIR_SCAN_REGS; i++) {
    s->pending[i] = -1;
  }
}

static void set_var(SirDebugScan *s, int n, int reg) {
  if (n >= 0 && (size_t)n < s->nlocals && reg >= 0 && s->slot_base[reg] != SIR_BASE_NONE) {
    s->vars[n].base = s->slot_base[reg];
    s->vars[n].offset = s->slot_off[reg];
  }
}

/* x86-64: leaq OFF(%rbp), %reg */
static void scan_leaq(SirDebugScan *s, const Insn *in) {
  const char *a = in->arg[0];
  size_t len = in->arg_len[0];
  int64_t off = 0;
  size_t used = 0;
  int reg = reg_of(in->arg[1], in->arg_len[1]);
  SirBase base = SIR_BASE_NONE;
  if (reg < 0) {
    return;
  }
  if (len > 0 && a[0] != '(' && !parse_int(a, len, &off, &used)) {
    used = len;
  }
  if (len - used == 6 && memcmp(a + used, "(%rbp)", 6) == 0) {
    base = SIR_BASE_FP;
  } else if (len - used == 6 && memcmp(a + used, "(%rsp)", 6) == 0) {
    base = SIR_BASE_SP;
  }
  s->slot_base[reg] = base;
  s->slot_off[reg] = off;
}

/* AArch64: add xR, x29, #N; add xR, x29, xT after mov xT, #N */
static void scan_add(SirDebugScan *s, const Insn *in) {
  int reg = reg_of(in->arg[0], in->arg_len[0]);
  SirBase base = arg_is(in, 1, "x29") ? SIR_BASE_FP : arg_is(in, 1, "sp") ? SIR_BASE_SP : SIR_BASE_NONE;
  int64_t off = 0;
  size_t used;
  bool known = false;
  if (reg < 0) {
    return;
  }
  if (base != SIR_BASE_NONE && in->nargs == 3) {
    int t = reg_of(in->arg[2], in->arg_len[2]);
    if (in->arg[2][0] == '#') {
      known = parse_int(in->arg[2] + 1, in->arg_len[2] - 1, &off, &used) && used == in->arg_len[2] - 1;
    } else if (t >= 0 && s->imm_known[t]) {
      off = s->imm[t];
      known = true;
    }
  }
  s->slot_base[reg] = known ? base : SIR_BASE_NONE;
  s->slot_off[reg] = off;
}

char sir_debug_scan_line(SirDebugScan *s, const char *line, size_t len, int *n) {
  Insn in;
  int marker = 0;
  char kind = sir_debug_find_marker(line, len, &marker);
  split(&in, line, len);
  if (kind == 'o' || kind == 'c' || kind == 't') {
    *n = marker;
    return kind;
  }
  if (op_is(&in, "leaq") && in.nargs == 2) {
    scan_leaq(s, &in);
  } else if (op_is(&in, "movq") && in.nargs == 2 && kind == 'v') {
    set_var(s, marker, reg_of(in.arg[0], in.arg_len[0]));
  } else if (op_is(&in, "adrp") && in.nargs == 2 && kind == 'v') {
    int a = reg_of(in.arg[0], in.arg_len[0]);
    if (a >= 0) {
      s->pending[a] = marker;
    }
  } else if (op_is(&in, "add") && in.nargs >= 2 && kind == 0) {
    scan_add(s, &in);
  } else if (op_is(&in, "mov") && in.nargs == 2) {
    int t = reg_of(in.arg[0], in.arg_len[0]);
    int64_t v = 0;
    size_t used;
    if (t >= 0) {
      s->imm_known[t] = in.arg[1][0] == '#' && parse_int(in.arg[1] + 1, in.arg_len[1] - 1, &v, &used);
      s->imm[t] = s->imm_known[t] ? v : 0;
    }
  } else if (op_is(&in, "str") && in.nargs == 2 && in.arg_len[1] > 2 && in.arg[1][0] == '[') {
    int a = reg_of(in.arg[1] + 1, in.arg_len[1] - 2);
    if (a >= 0 && s->pending[a] >= 0) {
      set_var(s, s->pending[a], reg_of(in.arg[0], in.arg_len[0]));
      s->pending[a] = -1;
    }
  }
  return 0;
}

bool sir_asm_is_trap(const char *line, size_t len) {
  Insn in;
  split(&in, line, len);
  return op_is(&in, "ud2") || op_is(&in, "brk");
}
