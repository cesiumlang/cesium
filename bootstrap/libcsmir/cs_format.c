/* The only translation unit where a format string is not a literal: the
 * v*printf calls behind the project's printf-style functions. It is built
 * without -Wformat-nonliteral; see cs_common.h.
 */
#include "cs_common.h"

int cs_vsnprintf(char *buf, size_t size, const char *fmt, va_list ap) {
  return vsnprintf(buf, size, fmt, ap);
}

void cs_buf_vprintf(CsBuf *b, const char *fmt, va_list ap) {
  va_list ap2;
  int n;
  va_copy(ap2, ap);
  n = vsnprintf(NULL, 0, fmt, ap2);
  va_end(ap2);
  if (n < 0) {
    CS_BUG("vsnprintf failed");
  }
  cs_buf_reserve(b, (size_t)n);
  n = vsnprintf(b->data + b->len, (size_t)n + 1, fmt, ap);
  if (n < 0) {
    CS_BUG("vsnprintf failed");
  }
  b->len += (size_t)n;
}

void cs_buf_printf(CsBuf *b, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  cs_buf_vprintf(b, fmt, ap);
  va_end(ap);
}
