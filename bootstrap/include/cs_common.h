/* Common definitions for stage 0 and libcsmir.
 *
 * Every project .c file includes this header first and nothing after it
 * except other project headers. It pulls in the allowed ISO C headers and
 * then cs_banned.h, so a banned function used anywhere fails to compile.
 * The rules are the stage 0 C dialect (cesiumlang/cesium#10).
 */
#ifndef CS_COMMON_H
#define CS_COMMON_H

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The one place a static assertion is spelled, so it can be swapped for a
 * compiler without _Static_assert. A negative array size is the portable
 * C99 form. */
#define CS_STATIC_ASSERT(cond, name) typedef char cs_static_assert_##name[(cond) ? 1 : -1]

#define CS_UNUSED(x) ((void)(x))

/* ---- overflow-checked sizes ------------------------------------------- */

/* Both abort on overflow: an allocation size is never open-coded. */
size_t cs_mul_size(size_t a, size_t b);
size_t cs_add_size(size_t a, size_t b);

/* ---- arena ------------------------------------------------------------- */

/* Memory that lives until the arena is freed. Blocks come from malloc. */
typedef struct CsArenaBlock CsArenaBlock;
typedef struct CsArena {
  CsArenaBlock *head;
} CsArena;

/* Zeroed, aligned for any object type. Aborts when memory runs out. */
void *cs_arena_new(CsArena *a, size_t size);
/* A NUL-terminated copy of len bytes. */
char *cs_arena_strn(CsArena *a, const char *s, size_t len);
char *cs_arena_str(CsArena *a, const char *s);
void cs_arena_free(CsArena *a);

/* ---- growable byte buffer --------------------------------------------- */

typedef struct CsBuf {
  char *data; /* always NUL-terminated once anything is appended */
  size_t len;
  size_t cap;
} CsBuf;

void cs_buf_put(CsBuf *b, const char *s, size_t len);
void cs_buf_puts(CsBuf *b, const char *s);
void cs_buf_putc(CsBuf *b, char c);
void cs_buf_printf(CsBuf *b, const char *fmt, ...);
void cs_buf_vprintf(CsBuf *b, const char *fmt, va_list ap);
/* Makes room for `extra` more bytes and the NUL. */
void cs_buf_reserve(CsBuf *b, size_t extra);
void cs_buf_free(CsBuf *b);

/* ---- formatting -------------------------------------------------------- */

/* vsnprintf for the project's own printf-style functions. The dialect bans
 * attributes, so a function that forwards its format cannot be marked as
 * printf-like, and -Wformat-nonliteral rejects the forwarding call. These
 * two (cs_format.c) are the only place a format is not a literal; that file
 * alone is built without that warning. */
int cs_vsnprintf(char *buf, size_t size, const char *fmt, va_list ap);

/* ---- files ------------------------------------------------------------- */

/* Reads a whole file into a malloc'd, NUL-terminated buffer. Returns false
 * and leaves *out untouched when the file cannot be read. */
bool cs_read_file(const char *path, char **out, size_t *len);
bool cs_write_file(const char *path, const char *data, size_t len);

/* ---- the platform (libcsmir/cs_platform.c) ----------------------------- */

/* Appends the current directory, in UTF-8. Returns false if the platform
 * cannot give it. */
bool cs_current_dir(CsBuf *out);

/* ---- internal errors --------------------------------------------------- */

/* An impossible state in the compiler itself: prints and aborts. */
void cs_bug(const char *file, int line, const char *what);
#define CS_BUG(what) cs_bug(__FILE__, __LINE__, what)

#include "cs_banned.h"

#endif
