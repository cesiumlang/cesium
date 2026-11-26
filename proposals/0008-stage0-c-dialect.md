+++
id = "P-0008"
type = "proposal"
status = "proposed"
+++

# Proposal P-0008: the stage 0 C dialect

This proposal decides the C in which stage 0 and `libcsmir` are written: a portable subset that every mainstream C compiler implements, the banned functions, the rules on undefined and implementation-defined behavior, the compiler flags and how CI enforces all of it. It carries the decision record cesiumlang/cesium#10, which was guidance and not accepted. emanspeaks accepts, changes or marks open each numbered point. Nothing in it is decided until he accepts it.

## Motivation

Stage 0 and `libcsmir` (#9, P-0007; #5, ADR 0002) must compile unchanged with any mainstream C compiler, not only `zig cc`, and should stay small, auditable and hard to misuse. "C11" alone is not enough: C11 made several features optional, and some compilers never implemented parts of earlier standards (MSVC never implemented C99's mandatory variable-length arrays). Stage 0's output must also be deterministic, for the reproducibility check of #5.

The banned functions are modeled on git's [`banned.h`](https://github.com/git/git/blob/master/banned.h), which bans `strcpy`, `strcat`, `strncpy`, `strncat`, `strtok`, `strtok_r`, `sprintf`, `vsprintf`, `gmtime`, `localtime`, `ctime`, `ctime_r`, `asctime`, `asctime_r` and `mktemp`, and on the Linux kernel's [deprecated interfaces](https://docs.kernel.org/process/deprecated.html): unbounded `strcpy`, `strncpy`'s non-terminating semantics, `strlcpy`, unchecked `simple_strto*` parsing, VLAs, implicit switch fall-through, zero-length and one-element arrays, open-coded arithmetic in allocation sizes. The record adapts them to ISO C.

## Design

### Scope and the rule

1. The dialect applies to stage 0 and `libcsmir` only. It does not constrain Cesium's language design or the real compiler, which is written in Cesium.
2. The rule is the intersection that every mainstream compiler implements, not a standard version, plus a list of banned functions.
3. The reference compilers are Clang (including `zig cc`), GCC, MSVC, and a small independent compiler such as TinyCC or cproc as a canary.
4. Code is ISO C that is valid as both C99 and C11, using only features every reference compiler implements, with no compiler extensions, only the ISO C standard library, and none of the banned functions.

### Allowed

5. The core C89 and C99 language: `//` comments, declarations mixed with code, `for (int i = 0; ...)`, designated initializers, compound literals, `static inline` functions, flexible array members.
6. The headers `<stddef.h>`, `<stdint.h>`, `<inttypes.h>`, `<stdbool.h>`, `<limits.h>`, `<stdarg.h>`, `<assert.h>`, `<string.h>`, `<stdio.h>`, `<stdlib.h>`, `<errno.h>`.
7. Fixed-width integer types (`int32_t`, `uint64_t`, ...), `size_t`, `ptrdiff_t`, `intptr_t` and `uintptr_t` (formally optional in ISO C but provided everywhere relevant), and the `PRIu64`-style format macros.
8. The bounded string and formatting functions `memcpy`, `memmove`, `memset`, `memcmp`, `strlen`, `strcmp`, `strncmp`, `memchr`, `strchr`, `snprintf`, `vsnprintf`, and `fprintf` and `printf` with constant format strings.
9. Numbers are parsed only with the `strtol`, `strtoul`, `strtoll` and `strtoull` family, checking `errno`, the end pointer and the range.
10. Headers have include guards.

### Not allowed

11. Optional or weakly supported standard features: variable-length arrays (`-Wvla`), `_Complex`, `<threads.h>`, `<stdatomic.h>` and `_Atomic`, `_Thread_local`, `_Generic`, `_Alignas` and `_Alignof`, `_Noreturn`, Annex K (the `*_s` functions), Annex L.
12. Static assertions only through one project macro, `CS_STATIC_ASSERT`, defined in a single header, so that its implementation can be swapped if a compiler lacks `_Static_assert`.
13. Compiler extensions: `__attribute__`, `__declspec`, `__builtin_*`, statement expressions, `typeof` and `__typeof__` (standard only from C23), computed `goto`, case ranges, zero-length and one-element trailing arrays (flexible array members instead), nested functions, `__int128`, inline assembly, `alloca`, `#pragma` (including `#pragma once`).
14. Non-ISO libraries: no POSIX or Win32 calls in stage 0, except through platform wrappers in one translation unit, `libcsmir/cs_platform.c`, the only code that calls the platform for what ISO C cannot do. The first is the current directory, for absolute source paths in debug information: `GetCurrentDirectoryW` on Windows, `getcwd` elsewhere (direction of emanspeaks, 2026-11-26: the capability matters more than strict ISO C, the dependence on libc is to be reduced as far as possible, and platform-specific wrappers will be needed; cesiumlang/vscode-cesium#47). File I/O uses `fopen`, `fread` and `fwrite`; the arena allocates large blocks with `malloc`.
15. Floating point is not needed in stage 0 and is avoided; `long double` entirely.
16. No implicit `switch` fall-through: every non-empty `case` ends in `break`, `return`, `continue` or `goto`. The portable fall-through annotations are an attribute or C23 syntax, neither of which is allowed, so there is no fall-through at all.

### Banned functions

17. These functions are banned:

| Banned | Why | Use instead |
| --- | --- | --- |
| `strcpy`, `strcat`, `strncpy`, `strncat`, `strlcpy`, `strlcat` | Unbounded copies, or (`strncpy`) results that may not be NUL-terminated | Explicit lengths with `memcpy`; project string helpers |
| `sprintf`, `vsprintf` | No bounds | `snprintf`, `vsnprintf` (check the return value) |
| `gets` | No bounds (removed in C11) | `fgets` / `fread` |
| `scanf`, `fscanf`, `sscanf` and `v*` variants | Unbounded `%s`, weak error reporting | Hand-written lexing, `strtol` family |
| `atoi`, `atol`, `atoll`, `atof` | No error detection | `strtol` family with checks |
| `strtok`, `strtok_r` | Hidden state, modifies input | Explicit scanning |
| `gmtime`, `localtime`, `ctime`, `asctime` (and `_r`), `time`, `clock` | Shared static buffers, and time makes output nondeterministic | Not needed in stage 0 |
| `tmpnam`, `mktemp`, `tmpfile` | Race-prone temporary files | The build system provides paths |
| `system`, `getenv`, `signal`, `raise` | Shell injection, nondeterminism, async hazards | Command-line arguments |
| `setjmp`, `longjmp` | Non-local control flow complicates review and cleanup | Error codes |
| `rand`, `srand` | Nondeterminism | Not needed |
| `realloc` used directly | Easy to leak or misuse on failure | Arena allocation; a checked wrapper if growth is needed |

18. No open-coded arithmetic in allocation sizes: sizes go through an overflow-checked helper (`cs_mul_size`, `cs_add_size`).
19. No `%p` and no printing of pointer values, and nothing else that depends on addresses, time or environment, so that stage 0's output is deterministic.
20. No `qsort` with comparators that can return equal for distinct items: `qsort` is not stable, so ties would make the output order nondeterministic. Comparators define a total order.

### Undefined and implementation-defined behavior

21. Code does not assume that `long` is 64-bit (it is 32-bit on Windows): it uses `int64_t` and `uint64_t`.
22. Code does not assume the signedness of `char`: it uses `unsigned char` or `uint8_t` for bytes.
23. Unsigned types where wraparound is intended; no signed overflow.
24. No shifts by negative amounts or by at least the operand width; no left shifts of negative values.
25. No type punning except through `memcpy`; no unaligned access; no reliance on struct padding or layout.
26. No dependence on the order of evaluation of function arguments or operands.
27. Pointer to integer conversion and back only through `uintptr_t`.
28. `memmove`, not `memcpy`, whenever regions may overlap.

### LLVM C API headers

29. `libcsmir` includes `llvm-c` headers (through `libcsllvm`). They are plain C but third-party code: if they trigger pedantic warnings under the flags below, their inclusion is confined to a single translation unit and warnings are relaxed for that file only, rather than weakening the rules everywhere; to be verified when the first build exists. The banned-functions header applies to project code only, so it is included after all system and LLVM headers.

### Flags

30. Clang, `zig cc` and GCC: `-std=c99` and `-std=c11` builds, both with `-pedantic-errors -Wall -Wextra -Wvla -Wconversion -Wformat=2 -Wformat-nonliteral -Wimplicit-fallthrough -Wstrict-prototypes -Wmissing-prototypes -Werror`.
31. MSVC: `/std:c11 /W4 /WX`.
32. Debug and test builds additionally with `-fsanitize=undefined,address` (Clang and `zig cc`).

### Enforcement in CI

33. A banned-functions header, `cs_banned.h`, in the style of git's `banned.h`, included last by a common project header, redefines each banned function as a macro that expands to an undefined identifier, so any use fails to compile. It is plain preprocessor code and works with every compiler, including TinyCC.
34. A simple source check fails the build if a banned identifier appears in project sources or if a `.c` file does not include the common header.
35. Stage 0 and `libcsmir` are built with `zig cc` and GCC on Linux, as both C99 and C11.
36. A compile-only check with a small independent compiler (TinyCC or cproc) is the canary for accidental extensions. Linking against `libcsllvm` may not be possible with every compiler; compiling every translation unit is the portability check.
37. An MSVC build on a Windows runner when practical. Stage 0 runs only in CI, so this is a portability safeguard, not a requirement for releases.
38. The stage 0 golden tests run under UBSan and ASan; fuzzing the MIR text parser is to be considered.

## Alternatives

- **A standard version as the rule ("C11").** The record rejects it: C11 made several features optional, and some compilers never implemented parts of earlier standards.
- **For the format-string conflict of the amendment of 2026-11-17** (see below), the amendment names two alternatives to what the code does: drop the printf-style helpers and build every message from literal-format `snprintf` calls at the call site; or allow a single project macro for the `format` attribute, defined in one header like `CS_STATIC_ASSERT`.

## Drawbacks

The record names none as such. It notes two costs in passing: with no fall-through annotation allowed, there is no fall-through at all (point 16); and linking against `libcsllvm` may not be possible with every compiler, so only compiling is checked everywhere (point 36). The amendment of 2026-11-17 names a third: calls to the project's own printf-style functions are not format-checked by the compiler.

## Open questions

The record lists none. The amendments raise these, for emanspeaks:

1. The handling of the conflict between `-Wformat=2`, `-Wformat-nonliteral` and the ban on attributes (2026-11-17, first comment), or one of its alternatives.
2. The three differences from the text in the enforcement comment of 2026-11-17: MSVC warning C4996 disabled; GCC compiling but not linking; fuzzing (since done, 2026-11-18).
3. The nesting limits chosen without guidance (2026-11-18): 128 levels for S0, 1024 for MIR text.
4. Whether the `llvm-c` headers need the confinement of point 29, which the record leaves to the first build with them.
5. Absolute source paths (point 14, 2026-11-26) make the debug information of Windows targets depend on the directory the compiler runs in, which point 19 rules out for stage 0's output: whether that is accepted for debug information, or the build directory can be overridden (as clang's `-fdebug-compilation-dir` does) for the reproducibility check of #5.

## What changed since the record

**2026-11-17: first code under this dialect (#60).** Stage 0 and `libcsmir` (about 6,600 lines with headers) build as both `-std=c99` and `-std=c11` with `zig cc` and the Clang flags of point 30, with `-Werror`. `cs_banned.h` is in place and included last by the common header.

- **One conflict in the rules as written.** `-Wformat=2` and `-Wformat-nonliteral` reject any function that forwards a format string to `vsnprintf` unless that function carries a `format` attribute, and attributes are not allowed. The project needs a few printf-style functions of its own (error reporting, the string buffer). What the code does: every call that forwards a format lives in one translation unit, `libcsmir/cs_format.c` (two small functions), and only that file is built with `-Wno-format-nonliteral`; `-Wno-missing-format-attribute` is added for all files, since `-Wformat=2` otherwise asks for the banned attribute. This mirrors point 29: relax one translation unit, not the rules. The alternatives are under Alternatives.
- **Not enforced yet** at that date: the source check for banned identifiers and for the common header; the GCC builds, the MSVC build, and the TinyCC or cproc compile check; ASan and fuzzing (debug builds ran with Zig's default UBSan).
- **Small notes.** `CS_STATIC_ASSERT` is defined with a negative array size, which works in C99 and C11 without `_Static_assert`. `mir_target_host()` chooses the default QBE target from predefined macros (`_WIN32`, `__x86_64__`, `__APPLE__`, `__aarch64__`, `__riscv`); they are not ISO C, and an unknown host gets no default and must name a target. Vendored QBE is not in the repository: it comes prebuilt from cesiumlang/devkits as `libqbe.a`.

**2026-11-17: enforcement status (#66, merged into `dev`).** It adds the checks of points 33 to 38 that were missing: the source check, `tools/check-c-dialect.py` (since rewritten in bash by #90, which rules out Python); GCC and Clang as C99 and C11 with the full flag set, TinyCC as the canary, MSVC with `/std:c11 /W4 /WX`, all compile only; the corpus under ASan and UBSan, up to QBE IL. All pass, and no source had to change for any compiler. Three points differ from the record, for emanspeaks to confirm:

1. MSVC warning C4996 is disabled: it deprecates `fopen` and other ISO C functions in favor of the Annex K variants this dialect bans.
2. GCC compiles but does not link, because the devkit's `libqbe.a` is built by `zig cc`. The record says "build" for GCC.
3. Fuzzing the MIR text parser is not done.

**2026-11-26: the current directory (cesiumlang/vscode-cesium#47).** Debugging on Windows needs CodeView line tables whose source paths are absolute, with backslashes: the linker makes a relative path absolute with forward slashes, which the Windows debugger engine does not match against a breakpoint's path. ISO C cannot ask for the current directory, and `zig cc` refuses the linker option that would set it. The record's rule (point 14) was the agents' guidance, not a decision; emanspeaks directed the exception now in point 14, confined to one file that the source check names (open question 5).

**2026-11-18: fuzzing (#69, merged into `dev`).** A mutation fuzzer for the MIR text parser and the S0 front end runs in CI on a build with ASan and UBSan. It found one bug: deeply nested S0 source overflowed the stack. The fix is a nesting limit, 128 levels for S0 (in `cesium0` and `cesium1`, with the same message) and 1024 for MIR text. The numbers were chosen without guidance (#9 names no limits) and are listed as such in #69 and in `bootstrap/README.md`.
