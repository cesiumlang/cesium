/* Arena, buffer, file and size helpers shared by stage 0 and libcsmir. */
#include "cs_common.h"

void cs_bug(const char *file, int line, const char *what) {
  fprintf(stderr, "%s:%d: internal error: %s\n", file, line, what);
  abort();
}

static void cs_out_of_memory(void) {
  fputs("out of memory\n", stderr);
  abort();
}

size_t cs_mul_size(size_t a, size_t b) {
  if (b != 0 && a > SIZE_MAX / b) {
    CS_BUG("size overflow");
  }
  return a * b;
}

size_t cs_add_size(size_t a, size_t b) {
  if (a > SIZE_MAX - b) {
    CS_BUG("size overflow");
  }
  return a + b;
}

/* ---- arena ------------------------------------------------------------- */

/* The strictest alignment any object needs, without _Alignof. */
typedef union CsMaxAlign {
  long long ll;
  void *p;
  double d;
  void (*fn)(void);
} CsMaxAlign;

struct CsArenaBlock {
  CsArenaBlock *next;
  size_t used;
  size_t cap;
  CsMaxAlign align; /* the payload starts at the next aligned address */
};

#define CS_ARENA_BLOCK ((size_t)64 * 1024)

static size_t cs_align_up(size_t n) {
  size_t a = sizeof(CsMaxAlign);
  return cs_add_size(n, a - 1) / a * a;
}

void *cs_arena_new(CsArena *a, size_t size) {
  size_t need = cs_align_up(size == 0 ? 1 : size);
  CsArenaBlock *b = a->head;
  unsigned char *p;
  if (b == NULL || b->cap - b->used < need) {
    size_t cap = need > CS_ARENA_BLOCK ? need : CS_ARENA_BLOCK;
    size_t header = cs_align_up(sizeof(CsArenaBlock));
    b = (CsArenaBlock *)malloc(cs_add_size(header, cap));
    if (b == NULL) {
      cs_out_of_memory();
    }
    b->next = a->head;
    b->used = 0;
    b->cap = cap;
    a->head = b;
  }
  p = (unsigned char *)b + cs_align_up(sizeof(CsArenaBlock)) + b->used;
  b->used += need;
  memset(p, 0, need);
  return p;
}

char *cs_arena_strn(CsArena *a, const char *s, size_t len) {
  char *p = (char *)cs_arena_new(a, cs_add_size(len, 1));
  if (len != 0) {
    memcpy(p, s, len);
  }
  p[len] = '\0';
  return p;
}

char *cs_arena_str(CsArena *a, const char *s) {
  return cs_arena_strn(a, s, strlen(s));
}

void cs_arena_free(CsArena *a) {
  CsArenaBlock *b = a->head;
  while (b != NULL) {
    CsArenaBlock *next = b->next;
    free(b);
    b = next;
  }
  a->head = NULL;
}

/* ---- buffer ------------------------------------------------------------ */

void cs_buf_reserve(CsBuf *b, size_t extra) {
  size_t need = cs_add_size(cs_add_size(b->len, extra), 1);
  if (need > b->cap) {
    size_t cap = b->cap == 0 ? 256 : b->cap;
    char *data;
    while (cap < need) {
      cap = cs_mul_size(cap, 2);
    }
    data = (char *)malloc(cap);
    if (data == NULL) {
      cs_out_of_memory();
    }
    if (b->len != 0) {
      memcpy(data, b->data, b->len);
    }
    data[b->len] = '\0';
    free(b->data);
    b->data = data;
    b->cap = cap;
  }
}

void cs_buf_put(CsBuf *b, const char *s, size_t len) {
  cs_buf_reserve(b, len);
  if (len != 0) {
    memcpy(b->data + b->len, s, len);
  }
  b->len += len;
  b->data[b->len] = '\0';
}

void cs_buf_puts(CsBuf *b, const char *s) {
  cs_buf_put(b, s, strlen(s));
}

void cs_buf_putc(CsBuf *b, char c) {
  cs_buf_put(b, &c, 1);
}

void cs_buf_free(CsBuf *b) {
  free(b->data);
  b->data = NULL;
  b->len = 0;
  b->cap = 0;
}

/* ---- files ------------------------------------------------------------- */

bool cs_read_file(const char *path, char **out, size_t *len) {
  FILE *f = fopen(path, "rb");
  CsBuf b = {NULL, 0, 0};
  char chunk[4096];
  size_t n;
  if (f == NULL) {
    return false;
  }
  while ((n = fread(chunk, 1, sizeof chunk, f)) != 0) {
    cs_buf_put(&b, chunk, n);
  }
  if (ferror(f)) {
    fclose(f);
    cs_buf_free(&b);
    return false;
  }
  fclose(f);
  if (b.data == NULL) {
    cs_buf_put(&b, "", 0);
  }
  *out = b.data;
  *len = b.len;
  return true;
}

bool cs_write_file(const char *path, const char *data, size_t len) {
  FILE *f = fopen(path, "wb");
  bool ok;
  if (f == NULL) {
    return false;
  }
  ok = len == 0 || fwrite(data, 1, len, f) == len;
  if (fclose(f) != 0) {
    ok = false;
  }
  return ok;
}
