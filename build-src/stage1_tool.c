/* A helper for the stage 1 test steps (build-src/tests.zig), built for the
 * host. cesium1 reads a whole unit on standard input, and its output is
 * compared with what cesium0 wrote; the build system has no step for
 * either with generated files.
 *
 *   stage1_tool cat <out> <in>...                 writes the inputs, one after
 *                                                 another, to <out>
 *   stage1_tool same <expected> <actual> [crlf]   fails unless <actual> has the
 *                                                 bytes of <expected>; with crlf,
 *                                                 every '\n' of <expected> as
 *                                                 "\r\n", which is how a C text
 *                                                 stream writes it on Windows
 *
 * Exit status: 0 on success, 1 on a difference or an I/O error.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Reads a whole file into memory; exits on an error. */
static char *read_file(const char *path, size_t *len) {
  FILE *f = fopen(path, "rb");
  char *data = NULL;
  size_t cap = 0;
  size_t n = 0;
  if (f == NULL) {
    fprintf(stderr, "stage1_tool: cannot read %s\n", path);
    exit(1);
  }
  for (;;) {
    size_t got;
    if (n == cap) {
      cap = cap == 0 ? 65536 : cap * 2;
      data = (char *)realloc(data, cap);
      if (data == NULL) {
        fputs("stage1_tool: out of memory\n", stderr);
        exit(1);
      }
    }
    got = fread(data + n, 1, cap - n, f);
    n += got;
    if (got == 0) {
      break;
    }
  }
  fclose(f);
  *len = n;
  return data;
}

static int cat(int argc, char **argv) {
  FILE *out = fopen(argv[2], "wb");
  int i;
  if (out == NULL) {
    fprintf(stderr, "stage1_tool: cannot write %s\n", argv[2]);
    return 1;
  }
  for (i = 3; i < argc; i++) {
    size_t len;
    char *data = read_file(argv[i], &len);
    if (fwrite(data, 1, len, out) != len) {
      fprintf(stderr, "stage1_tool: cannot write %s\n", argv[2]);
      return 1;
    }
    free(data);
  }
  return fclose(out) == 0 ? 0 : 1;
}

static int same(const char *expected_path, const char *actual_path, int crlf) {
  size_t elen;
  size_t alen;
  char *e = read_file(expected_path, &elen);
  char *a = read_file(actual_path, &alen);
  size_t i = 0;
  size_t j = 0;
  size_t line = 1;
  while (i < elen && j < alen) {
    if (crlf && e[i] == '\n') {
      if (a[j] != '\r') {
        break;
      }
      j++;
      if (j == alen) {
        break;
      }
    }
    if (e[i] != a[j]) {
      break;
    }
    if (e[i] == '\n') {
      line++;
    }
    i++;
    j++;
  }
  if (i == elen && j == alen) {
    return 0;
  }
  fprintf(stderr, "stage1_tool: %s differs from %s at line %lu of the latter\n", actual_path, expected_path,
          (unsigned long)line);
  return 1;
}

int main(int argc, char **argv) {
  if (argc >= 3 && strcmp(argv[1], "cat") == 0) {
    return cat(argc, argv);
  }
  if ((argc == 4 || argc == 5) && strcmp(argv[1], "same") == 0) {
    return same(argv[2], argv[3], argc == 5 && strcmp(argv[4], "crlf") == 0);
  }
  fputs("usage: stage1_tool cat <out> <in>... | stage1_tool same <expected> <actual> [crlf]\n", stderr);
  return 1;
}
