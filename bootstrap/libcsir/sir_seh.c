/* Windows x64 unwind data for QBE's functions on COFF targets
 * (cesiumlang/vscode-cesium#47).
 *
 * QBE writes none, so a debugger takes each function for a leaf, whose
 * return address is at the top of the stack, and finds no caller. This
 * describes the prologue QBE writes with LLVM's .seh_ directives, from which
 * the assembler makes .pdata and .xdata: endbr64 and the stores of variadic
 * arguments, then pushq %rbp and movq %rsp, %rbp where there is a frame
 * pointer, subq $N, %rsp and the pushes of saved registers. QBE moves rsp
 * around each call, so a function with a frame pointer is unwound from rbp:
 * the frame is set after the pushes, at rbp less all that lies below it.
 */
#include "sir_core.h"

/* the largest frame offset an unwind record holds, in bytes */
#define SEH_MAX_FRAME_OFFSET 240

typedef struct {
  const char *s;
  size_t len; /* without the line ending */
  size_t next; /* where the line after it starts */
} SehLine;

static SehLine line_at(const char *body, size_t len, size_t at) {
  SehLine l;
  const char *nl = at < len ? memchr(body + at, '\n', len - at) : NULL;
  l.s = body + at;
  l.len = nl == NULL ? len - at : (size_t)(nl - l.s);
  l.next = nl == NULL ? len : at + l.len + 1;
  if (l.len > 0 && l.s[l.len - 1] == '\r') {
    l.len--;
  }
  return l;
}

static bool is(SehLine l, const char *text) {
  return l.len == strlen(text) && memcmp(l.s, text, l.len) == 0;
}

static bool starts(SehLine l, const char *prefix) {
  size_t n = strlen(prefix);
  return l.len >= n && memcmp(l.s, prefix, n) == 0;
}

static bool ends(SehLine l, const char *suffix) {
  size_t n = strlen(suffix);
  return l.len >= n && memcmp(l.s + l.len - n, suffix, n) == 0;
}

/* The N of subq $N, %rsp, or 0 if the line is not one. */
static uint64_t stack_alloc(SehLine l) {
  uint64_t n = 0;
  size_t i = strlen("\tsubq $");
  if (!starts(l, "\tsubq $") || !ends(l, ", %rsp")) {
    return 0;
  }
  for (; i < l.len && l.s[i] >= '0' && l.s[i] <= '9'; i++) {
    n = n * 10 + (uint64_t)(l.s[i] - '0');
  }
  return i == l.len - strlen(", %rsp") ? n : 0;
}

static void put_line(CsBuf *out, SehLine l) {
  cs_buf_put(out, l.s, l.len);
  cs_buf_putc(out, '\n');
}

/* Walks the prologue at the start of body, and with out writes it with its
 * directives; fits says whether the frame is set after the pushes. Returns
 * where the prologue ends. */
static size_t walk(CsBuf *out, const char *body, size_t len, bool fits, bool *frame, uint64_t *below) {
  size_t at = 0;
  bool started = false; /* past endbr64 and the variadic stores */
  *frame = false;
  *below = 0;
  while (at < len) {
    SehLine l = line_at(body, len, at);
    SehLine after = line_at(body, len, l.next);
    uint64_t n = stack_alloc(l);
    if (!started && (is(l, "\tendbr64") || (starts(l, "\tmovq %") && ends(l, "(%rsp)")))) {
      if (out != NULL) {
        put_line(out, l);
      }
      at = l.next;
    } else if (!started && is(l, "\tpushq %rbp") && is(after, "\tmovq %rsp, %rbp")) {
      *frame = true;
      started = true;
      if (out != NULL) {
        put_line(out, l);
        cs_buf_puts(out, "\t.seh_pushreg %rbp\n");
        put_line(out, after);
      }
      at = after.next;
      if (!fits) {
        if (out != NULL) {
          cs_buf_puts(out, "\t.seh_setframe %rbp, 0\n");
        }
        return at;
      }
    } else if (n != 0 && *below == 0) {
      started = true;
      *below += n;
      if (out != NULL) {
        put_line(out, l);
        cs_buf_printf(out, "\t.seh_stackalloc %" PRIu64 "\n", n);
      }
      at = l.next;
    } else if (starts(l, "\tpushq %")) {
      started = true;
      *below += 8;
      if (out != NULL) {
        put_line(out, l);
        cs_buf_printf(out, "\t.seh_pushreg %.*s\n", (int)(l.len - strlen("\tpushq ")), l.s + strlen("\tpushq "));
      }
      at = l.next;
    } else {
      break;
    }
  }
  return at;
}

void sir_seh_prologue(CsBuf *out, const char *name, const char *body, size_t len) {
  bool frame;
  uint64_t below;
  size_t end;
  bool fits;
  walk(NULL, body, len, true, &frame, &below);
  fits = !frame || (below <= SEH_MAX_FRAME_OFFSET && below % 16 == 0);
  cs_buf_printf(out, ".seh_proc %s\n", name);
  end = walk(out, body, len, fits, &frame, &below);
  if (frame && fits) {
    cs_buf_printf(out, "\t.seh_setframe %%rbp, %" PRIu64 "\n", below);
  }
  cs_buf_puts(out, "\t.seh_endprologue\n");
  cs_buf_put(out, body + end, len - end);
}
