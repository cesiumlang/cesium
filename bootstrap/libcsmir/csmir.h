/* libcsmir's coarse C API (cesiumlang/cesium#9, "cesium1 and the coarse
 * libcsmir API"): only pointers, sizes and int, no structs by value, no
 * callbacks, nothing the caller must free. This is what cesium1 calls.
 *
 * Only the QBE backend exists so far (ADR 0004, cesiumlang/cesium#7).
 */
#ifndef CS_CSMIR_H
#define CS_CSMIR_H

#include <stddef.h>

#define CS_BACKEND_QBE 0
#define CS_BACKEND_LLVM 1

/* Prepares the backends. Returns 0. */
int cs_backend_init(void);

/* Compiles core MIR text.
 *
 *   backend    CS_BACKEND_QBE writes assembly text to out_path, and the QBE
 *              IL it was made from to "<out_path>.ssa".
 *   triple     the target: "" or NULL for the machine this runs on, a QBE
 *              target name ("amd64_sysv"), or a triple starting with
 *              <arch>-<os> ("x86_64-linux-musl").
 *   cpu, features, opt_level   unused by the QBE backend.
 *
 * Returns nonzero on an error; cs_last_error() describes it.
 *
 * With the QBE backend a successful call does NOT return: the pinned QBE is
 * run in-process as the program it is, and it ends the process with status
 * 0 once the assembly is written. Call it last.
 */
int cs_compile_mir(const char *mir, size_t len, int backend, const char *triple, const char *cpu,
                   const char *features, int opt_level, const char *out_path);

/* The message of the last failed call. Owned by the library. */
const char *cs_last_error(void);

#endif
