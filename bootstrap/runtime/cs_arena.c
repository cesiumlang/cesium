/* The runtime behind S0's alloc(count) (cesiumlang/cesium#9): a bump arena
 * that never frees. A program built by stage 0 is a short-lived process;
 * exit frees everything.
 *
 * Linked into every program stage 0 compiles. Memory is zeroed. When the
 * system has no more memory the program stops with a message: S0 has no way
 * to report the failure (allocation failure is an open question,
 * cesiumlang/cesium#17).
 */
#include "cs_common.h"

void *cs_arena_alloc(size_t size, size_t align);

#define CS_RT_BLOCK ((size_t)1 << 20)

static unsigned char *block;
static size_t block_used;
static size_t block_cap;

void *cs_arena_alloc(size_t size, size_t align) {
  size_t start;
  unsigned char *p;
  if (align == 0) {
    align = 1;
  }
  if (size == 0) {
    size = 1;
  }
  /* malloc's memory is aligned for any scalar, so offsets within a block
   * that are multiples of align are aligned too */
  start = cs_add_size(block_used, align - 1) / align * align;
  if (block == NULL || start > block_cap || block_cap - start < size) {
    size_t cap = size > CS_RT_BLOCK ? size : CS_RT_BLOCK;
    block = (unsigned char *)malloc(cap);
    if (block == NULL) {
      fputs("out of memory\n", stderr);
      exit(1);
    }
    block_cap = cap;
    start = 0;
  }
  p = block + start;
  block_used = cs_add_size(start, size);
  memset(p, 0, size);
  return p;
}
