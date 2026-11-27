/* The platform wrappers: the only code that calls the platform, for what ISO
 * C cannot do (P-0008, point 14). Where the platform offers it, a wrapper
 * calls the system rather than libc. */
#if !defined(_WIN32)
  /* getcwd is POSIX; strict C99 headers declare it only with this */
  #define _POSIX_C_SOURCE 200809L
  #include <unistd.h>
#endif
#include "cs_common.h"

#if defined(_WIN32)

/* kernel32's, declared here: <windows.h> is not pedantic C, and x64 has one
 * calling convention. */
unsigned long GetCurrentDirectoryW(unsigned long size, uint16_t *buf);

/* Appends UTF-16 as UTF-8; an unpaired surrogate becomes U+FFFD. */
static void put_utf8(CsBuf *out, const uint16_t *s, size_t n) {
  size_t i;
  for (i = 0; i < n; i++) {
    uint32_t c = s[i];
    if (c >= 0xd800 && c <= 0xdbff && i + 1 < n && s[i + 1] >= 0xdc00 && s[i + 1] <= 0xdfff) {
      c = 0x10000 + ((c - 0xd800) << 10) + (uint32_t)(s[i + 1] - 0xdc00);
      i++;
    } else if (c >= 0xd800 && c <= 0xdfff) {
      c = 0xfffd;
    }
    if (c < 0x80) {
      cs_buf_putc(out, (char)c);
    } else if (c < 0x800) {
      cs_buf_putc(out, (char)(0xc0 | (c >> 6)));
      cs_buf_putc(out, (char)(0x80 | (c & 0x3f)));
    } else if (c < 0x10000) {
      cs_buf_putc(out, (char)(0xe0 | (c >> 12)));
      cs_buf_putc(out, (char)(0x80 | ((c >> 6) & 0x3f)));
      cs_buf_putc(out, (char)(0x80 | (c & 0x3f)));
    } else {
      cs_buf_putc(out, (char)(0xf0 | (c >> 18)));
      cs_buf_putc(out, (char)(0x80 | ((c >> 12) & 0x3f)));
      cs_buf_putc(out, (char)(0x80 | ((c >> 6) & 0x3f)));
      cs_buf_putc(out, (char)(0x80 | (c & 0x3f)));
    }
  }
}

bool cs_current_dir(CsBuf *out) {
  uint16_t small[260];
  uint16_t *buf = small;
  unsigned long n = GetCurrentDirectoryW(260, small);
  bool ok;
  if (n > 260) {
    /* n counts the NUL it needs */
    unsigned long size = n;
    buf = (uint16_t *)malloc(cs_mul_size(size, sizeof *buf));
    if (buf == NULL) {
      return false;
    }
    n = GetCurrentDirectoryW(size, buf);
    if (n >= size) {
      n = 0;
    }
  }
  ok = n != 0;
  if (ok) {
    put_utf8(out, buf, n);
  }
  if (buf != small) {
    free(buf);
  }
  return ok;
}

#else

bool cs_current_dir(CsBuf *out) {
  CsBuf buf = {NULL, 0, 0};
  size_t size;
  for (size = 256; size <= 65536; size *= 2) {
    cs_buf_reserve(&buf, size);
    if (getcwd(buf.data, size) != NULL) {
      cs_buf_puts(out, buf.data);
      cs_buf_free(&buf);
      return true;
    }
    if (errno != ERANGE) {
      break;
    }
  }
  cs_buf_free(&buf);
  return false;
}

#endif
