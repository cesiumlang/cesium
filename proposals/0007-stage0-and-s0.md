+++
id = "P-0007"
type = "proposal"
status = "proposed"
+++

# Proposal P-0007: stage 0 and the S0 subset

This proposal decides what stage 0 is, the bootstrap ladder from it to the real compiler, the S0 subset of Cesium that stage 0 compiles and its mapping onto core SIR, how stage 0 and `cesium1` are built, and how they are tested. It carries the decision record cesiumlang/cesium#9, which was guidance and not accepted. emanspeaks accepts, changes or marks open each numbered point. Nothing in it is decided until he accepts it.

## Motivation

Stage 0 is the root of the bootstrap chain (#5, ADR 0002). It has to be small, buildable with any mainstream C compiler, and frozen, and the language it accepts has to be enough to write stage 1 in. The record builds stage 0 around the core level of the semantic IR (#8, P-0006), so that the path every later compiler uses is exercised from the first day.

Why C rather than Zig (see #5): a frozen language for the root of the bootstrap chain, native use of LLVM's C API when it arrives, and buildability with any mainstream C compiler. Zig's release churn is confined to `build.zig`. The C dialect's limitations (#10, P-0008) apply only to stage 0 and `libcsir`; they do not affect Cesium's language design or the real compiler.

The model is early C. C grew out of B in the early 1970s by first adding types (`char` and `int`, pointers, arrays); structs arrived around 1973 because rewriting the Unix kernel in C was impractical without them; `unsigned`, `long`, `typedef`, `union` and `enum` came later in the decade (dates approximate; see Dennis Ritchie, *The Development of the C Language*). S0 follows the same reasoning: minimal types, pointers, arrays, and structs as the one feature that makes a large systems program bearable.

## Design

Points 1 to 5 are the record's rules 1 to 5 and keep their numbers; the specification cites them as "rule N".

### Rules

1. S0 is a strict subset of Cesium. Every S0 program is a valid Cesium program with the same meaning. Later compilers must still be able to compile the frozen stage 1, so CI's from-source check can run down the whole chain.
2. Features are added to S0 only when stage 1's code genuinely needs them and implementing them in stage 0 is cheaper than writing around them.
3. S0 maps (nearly) one-to-one onto core SIR. If a proposed S0 feature needs a nontrivial rewrite, it belongs in S1 instead.
4. Stage 0 output may be naive. There is no optimization design in stage 0.
5. Stage 0's C follows the stage 0 C dialect (#10, P-0008): valid as both C99 and C11, only features every mainstream compiler implements, no compiler extensions, only the ISO C standard library, no banned functions; built and tested in CI with several compilers and with UBSan and ASan. Vendored QBE is third-party code and exempt.

### Stage 0 and the ladder

6. Stage 0 is a small compiler written in portable C and compiled by `zig cc` in CI. It runs only in CI and has one job: compile stage 1. It accepts S0, a deliberately tiny subset of Cesium. Code it generates does not need to be efficient.
7. Stage 0 is built around the core level of SIR. S0 is designed together with core SIR so that S0 is essentially a readable notation for it. The core SIR parser, verifier and lowerings (`libcsir`) are the first implementation of the path every later compiler uses.
8. Stage 0 and `cesium1` start on QBE (vendored, MIT), per #7. No LLVM build is needed to begin; the LLVM backend and `libcsllvm` are added later, and from then on both backends are checked against each other by differential testing.
9. The bootstrap ladder:

| Layer | Written in | Compiles | Notes |
| --- | --- | --- | --- |
| Stage 0 | Portable C | S0 | Tiny S0 frontend and `libcsir` (QBE first); CI only |
| Stage 1 (`cesium1`) | S0 | S1 | Frontend only: parses S1, emits core SIR text, hands it to `libcsir`. Verbose by design; frozen once the next layer works |
| Real compiler | S1 | Full Cesium | Reimplements SIR (both levels) and both lowerings in Cesium; borrow checking, generics, math operators, freestanding support (LLVM); starts on QBE, adds LLVM through `libcsllvm` |

10. Convenience features (for loops, switch, tagged unions, strings, modules, floats, ...) are implemented in stage 1, written in S0, not in stage 0, and are defined as rewrites into core SIR. More layers are added only if a later compiler genuinely needs features its predecessor cannot compile. (Modules: see the direction of 2026-11-22 below, which brings `import` into S0.)

### S0

11. S0 contains the following. The table is the record's, with the amendment of 2026-11-16 and the later directions applied and marked; the directions are not decisions (see "What changed since the record").

| Area | S0 |
| --- | --- |
| Types | `u8`, `i32`, `u32`, `i64`, `u64`, `uword`; pointers `#T`, a pointer through which the data is not modified, everywhere (amendment of 2026-11-16) (`~T` parsed and treated the same: no borrow checking); pointer-to-pointer (`##T`); fixed arrays `T name[N]`. A pointer is set with `:=` or another address-context assignment, never with plain `=`, which assigns through it (amendment of 2026-11-16) |
| Data | `struct` with fields only: no methods, no struct literals (assign fields one at a time) |
| Control flow | `if` / `else`, `while`, `break`, `continue`, `return` |
| Functions | `T = name(params) { ... }`, single return value, `void`, recursion |
| Expressions | `+ - * / %`, comparisons, `&& \|\| !`, bitwise `& \|` and `><` (XOR), shifts `<< >>`, indexing `p[i]`, address-of `#x`, field access `a.b` (auto-dereferences pointers; no dereference operator, amendment of 2026-11-16), casts `as`, `sizeof`. Direction (#34): numeric conversion is `T(x)`, such as `u64(x)`; `as` is bit reinterpretation or a pointer cast |
| Literals | integers, characters (`'A'`), strings as NUL-terminated `#u8`. Directions: a string literal is a static `str` that converts to `#u8` (#39); `true` and `false` are `u8` constants (#38); a literal with no context is `i32`, else `i64` (#33) |
| Memory | `alloc(count)` only: a bump arena that never frees (stage 1 is a short-lived process; exit frees everything). Follows the specification's meaning of `alloc` (element count, element type from the declared target) |
| Globals | global variables and `const` globals (constants also stand in for enums) |
| Outside world | `extern name = import("lib") { ... }` blocks declaring C ABI functions: a few libc functions (`open`, `read`, `write`, `close`, `exit`) and the coarse `libcsir` API. Library names in double quotes: direction (#19); the record wrote `import('lib')` |
| Compilation unit | Direction of 2026-11-22: one entry file, named by the build script `build.csm`, and the files its imports name, followed from it; the S0 import is `import(ns, "path/to/file.csm")` (#19). The record had "multiple files compiled as one unit with shared globals; no `import` of Cesium modules" |
| Entry point | hosted `main` (`i32 = main(i32 argc, ##u8 argv)`) |

12. `i32` and `u32` are in S0 for C ABI correctness: passing `i64` where C expects `int` happens to work on x86-64 but breaks on 32-bit ARM and PowerPC, where it occupies two registers.
13. S0's arithmetic follows core SIR's defined semantics (wrapping integers, defined shifts and division by zero; P-0006), so stage 0's output means the same thing on both backends.
14. Each S0 construct maps onto core SIR as follows, and every statement also carries its source location, so the backends can emit debug line information.

| S0 | Core SIR |
| --- | --- |
| `T name = e;` (local) | `local %name: T = e` |
| `x = e;` (local) | `set %x = e` |
| `p[i] = e;`, `p.f = e;` | `store index(p, i), e`, `store field(p, f), e` |
| `p[i]`, `p.f` (read) | `load(index(p, i))`, `load(field(p, f))` |
| `#x` | `addr(%x)` |
| `if` / `else`, `while`, `break`, `continue`, `return` | the same structured statements |
| `a + b`, `a < b`, `a >< b`, ... | `add(a, b)`, `lt(a, b)`, `xor(a, b)`, ... |
| `a && b`, `a \|\| b`, `!a` | `land(a, b)`, `lor(a, b)`, `lnot(a)` |
| `e as T` | `as(T, e)` |
| `sizeof(T)` | `sizeof(T)` (resolved by the backend) |
| `f(args)` | `call @f(args)` |
| `"text"` | `const @str.N: [len+1 x u8] = "text\00"` and its address |
| `alloc(n)` (target `#T`) | `as(ptr<T>, call @cs_arena_alloc(mul(n, sizeof(T)), alignof(T)))` |
| `struct S { ... }` | `struct S { ... }` |
| `extern ... import(...) { ... }` | `extern func @name(...) -> T callconv(c)` |

After the amendment and the directions, the table has no row for a pointer set with `:=` or `#=`, for a conversion `T(x)` (#34), or for `import(ns, "path")`; their mappings are not recorded.

15. The arena allocator `cs_arena_alloc` is a tiny runtime function shipped with stage 0, written in C inside `libcsir` (large blocks from `malloc`), or later in S0 itself.
16. Not in S0: `for`, ranges, `switch` and `case`, `enum`, unions and `typeof`, `slice`, `list`, `dict` and `str`, multiple return values, `defer`, `error` types and `catch` and `throw`, floating point, `free` and `realloc`, methods and `this`, generics, traits, inheritance, operator overloading, the math operators (`^`, `::`, `@`, context-aware `*`, postfix `~`), SIMD, `comptime`, string interpolation, `with`, inline `asm`, `volatile` and `atomic`. The record also listed "modules and `import` of Cesium code", which the direction of 2026-11-22 removes.
17. Stage 1's source simulates what it needs by hand: tag fields plus casts for AST nodes, hand-written hash tables over pointers, error codes as return values, NUL-terminated strings with explicit lengths where needed.
18. Floating point is not needed in S0: the compiler can carry float literals as text. Converting decimal text to exact IEEE bits (LLVM IR's decimal `double` constants must be exactly representable, otherwise hex form is required) belongs in a layer written in S1, which has `f64`.

### Stage 0 pipeline and components

19. Phase 1 (QBE only): S0 source files, lexer, recursive-descent or Pratt parser, minimal type checking, core SIR (in memory), then `libcsir`: verify, lower to QBE IL, vendored QBE (called in-process), assembly file; `build.zig` assembles and links with `zig cc` to make `cesium1`.
20. Later (both backends): `libcsir` gains the SIR to LLVM IR lowering; the same stage 0 can produce `cesium1` through either backend, and both builds must behave identically (differential testing).
21. Stage 0 consists of, all in portable C plus vendored QBE: the S0 frontend (lexer, parser, type checking, S0 to SIR mapping); `libcsir` (core SIR data structures as arena-allocated tagged unions, text parser, printer, verifier, the QBE lowering with the QBE-side layout computation and narrow-integer handling, later the LLVM lowering, and the arena runtime); vendored QBE, with its entry point renamed so it can be called in-process. Stage 0 does not spawn processes; assembling and linking are done by the build system.
22. Stage 0 can print the SIR it produces, which gives golden tests for both the frontend and `libcsir`. The record's rough size estimate: a few thousand lines of C for the frontend and `libcsir`, plus QBE (about 12 to 14 thousand lines, vendored).

### `cesium1` and the coarse `libcsir` API

23. Stage 1 is only a frontend: it emits core SIR text and never generates QBE IL or LLVM IR itself. It calls a coarse, S0-friendly C ABI exported by `libcsir`: only pointers, `uword` and `i32`; no by-value structs, callbacks, variadics, or out-parameters that need freeing. As the record sketches it:

```cesium
extern csir = import('csir') {
    i32 = cs_backend_init();                       // initialize backends (and LLVM targets, once present)
    i32 = cs_compile_sir(#u8 sir, uword len,       // core SIR text in
                         i32 backend,              // 0 = QBE (writes assembly), 1 = LLVM (writes an object)
                         #u8 triple, #u8 cpu, #u8 features,
                         i32 opt_level, #u8 out_path);
    i32 = cs_lld_link(i32 argc, ##u8 argv, #i32 can_run_again);  // available once libcsllvm is linked
    #u8 = cs_last_error();                         // owned by the library; never freed
}
```

24. Self-containment of `cesium1` comes in two phases (#7). First, QBE only: `cesium1` writes assembly, and `build.zig` assembles and links it with `zig cc`; acceptable because `cesium1` only runs in CI. Then, with `libcsllvm`: `cesium1` assembles QBE output through LLVM's integrated assembler (`module asm` wrapping, through `cs_compile_ir`) and links with in-process LLD, or uses the LLVM backend directly. Tested 2026-11-16: wrapped QBE-style assembly assembled through the LLVM C API and linked with in-process LLD produced a working static executable.
25. The real compiler reimplements SIR and both lowerings in Cesium and calls `libcsllvm` directly, dropping `libcsir`.

### S1 and the real compiler

26. Stage 1 is written in S0 and compiles S1, a comfortable language for writing the real compiler. Each S1 feature is defined as a rewrite into core SIR. The candidates, to be confirmed when stage 1 is designed: `for` with ranges and collection iteration; `switch` and `case` with values, ranges, `else`; `enum` (including backed enums); type unions with `switch (typeof(x))` (AST nodes); `str` and `slice[T]`; built-in `list[T]` and `dict[K,V]`; multiple return values; `defer`; `error` types, `T|Error` returns, `catch (err) { case (...) }` (return-based only; `throw` later); `namespace` and `import` of Cesium modules, and `private`; `f32` and `f64`, including exact decimal-to-IEEE conversion for literals (requires SIR float operations); a real allocator (`free`, `realloc`); possibly methods with `this`, if the real compiler's code needs them.
27. The real compiler, written in S1, implements the full language, including borrow checking (which it applies to its own source), generics, the math operators, SIMD, the numerics level of SIR, freestanding support (#6, LLVM backend), and both lowerings in Cesium.

### Testing

28. A corpus of S0 programs with expected outputs (golden tests) for stage 0.
29. Golden SIR tests: S0 source to expected SIR text; SIR text to expected behavior (exercises `libcsir` independently of the frontend).
30. Differential testing once the LLVM lowering exists (#7): the same corpus through QBE, LLVM -O0 and LLVM -O2 must produce identical output; generated layout tests (`sizeof`, `alignof`, field offsets) compared across both backends and against equivalent C compiled by `zig cc`; generated C ABI call tests in both directions.
31. Stage 0 and `libcsir` are built as both C99 and C11 with `zig cc` and GCC, compile-checked with a small independent compiler (TinyCC or cproc), and run under UBSan and ASan (#10); fuzzing the SIR text parser is to be considered.
32. `cesium1` built by stage 0 must compile the S1 test corpus and the real compiler.
33. Every later compiler must still compile frozen stage 1 (enforces point 1).
34. The real compiler must rebuild itself with identical output.

### The record's sample program

Illustrative only, as the record says; shown as the record has it. The amendments and directions change it: `:=` for `t` and `buf` and an address-context assignment for `t.start` (2026-11-16); `~Token`, `~u8` and `~u8 buf` in the test copy (2026-11-17); `import("c")` (#19); `u8('0' + (v % 10))` and `u64(t.len)` (#34).

```cesium
extern libc = import('c') {
    i64 = write(i32 fd, #u8 buf, uword len);
}

const i64 TOK_IDENT = 1;
const i64 TOK_NUMBER = 2;

struct Token {
    i64 kind;
    #u8 start;
    uword len;
}

uword = cstr_len(#u8 s) {
    uword n = 0;
    while (s[n] != 0) {
        n = n + 1;
    }
    return n;
}

void = print(#u8 s) {
    write(1, s, cstr_len(s));
}

// Write an unsigned integer as decimal text into buf; returns the length.
uword = fmt_u64(u64 v, #u8 buf) {
    u8 tmp[20];
    uword n = 0;
    if (v == 0) {
        buf[0] = '0';
        return 1;
    }
    while (v != 0) {
        tmp[n] = ('0' + (v % 10)) as u8;
        v = v / 10;
        n = n + 1;
    }
    uword i = 0;
    while (i < n) {
        buf[i] = tmp[n - 1 - i];
        i = i + 1;
    }
    return n;
}

i32 = main(i32 argc, ##u8 argv) {
    #Token t = alloc(1);
    t.kind = TOK_IDENT;
    t.start = "hello";
    t.len = cstr_len(t.start);

    #u8 buf = alloc(32);
    uword n = fmt_u64(t.len as u64, buf);
    buf[n] = '\n';
    buf[n + 1] = 0;
    print("token length: ");
    print(buf);
    return 0;
}
```

## Alternatives

- **Stage 0 as only a SIR assembler, with stage 1 written by hand in SIR text.** Considered and rejected by the record: writing a compiler frontend in IR text is close to writing it in assembly, and would push SIR's design toward hand-writability instead of its real job (shapes, aliasing, vectors, fusion); it would freeze the SIR text format, since every later compiler would have to keep accepting SIR v0; and it saves no parsing work, since stage 1 must parse Cesium source regardless. The chosen design keeps the benefits (SIR exercised from day one, composite features defined as rewrites into the core) while stage 1 stays readable and SIR stays free to change.
- **Stage 0 in Zig.** Not taken, for the reasons under Motivation (#5).

## Drawbacks

The record names none.

## Open questions

The record's open questions, with what has happened to each:

1. **File extension.** The record found `.cs`, `.cesium` and `.czm` in use. Decided by the amendment of 2026-11-16: `.csm` for source, `.csi` for module interfaces (#14).
2. **Declaration and pointer syntax.** Pointee access and `#T` in a declaration were confirmed by the amendment of 2026-11-16: dereference by default, and `#T` is a pointer everywhere. Still open: the result type of comparisons (`u8`, since Cesium has no `bool`; #38 leaves the value of comparisons to its proposal) and the type of a literal with no context (direction in #33: `i32`, else `i64`).
3. **Core SIR text syntax:** see P-0006.
4. **Which QBE version to vendor**, and its line-debug support and Windows output. The amendment of 2026-11-17 reports that the devkit pins QBE at `180d2eb` (upstream master, 2026-11-09), which has the `amd64_win` target and documents `dbgloc`.

Raised by the amendments and still open:

5. The findings of 2026-11-17 about the ownership rules in S0: writes through `#T`, `~=` and prefix `~x`, and no `free` against rule 9 of #11 (tracked in #74).
6. `null` and pointer comparison in S0 (findings of 2026-11-17 and its correction; #26, #43).
7. The libc functions stage 1 may call: `write` is POSIX, and its signature is wrong on Windows; per-platform signatures or a small C shim in `libcsir` (finding 6 of 2026-11-17).
8. What an S0 `import` is, what subset of Cesium a build script may use in S0, and what `set_entrypoint()` looks like (#19, #47 item 8, #104).

## What changed since the record

Amendments on #9:

- **2026-11-16: amendments needed.** Open question 1 decided: `.csm` and `.csi` (#14). Ownership rules (#11): a pointer is never initialized with plain `=`, which assigns through it; in the sample, `#Token t = alloc(1);` and `#u8 buf = alloc(32);` become `:=`, and setting the pointer field `t.start = "hello";` needs an address-context assignment; because S0 is a strict subset of Cesium, the sample and the S0 table need revising even though stage 0 does not enforce ownership. Open question 2: dereference by default is confirmed, and `#T` in a declaration is a pointer (immutable data) everywhere.
- **2026-11-17: stage 0 first implementation (#60).** `cesium0` compiles S0 to assembly through core SIR and QBE; the sample, amended, compiles and runs, and its `cstr_len` lowers to exactly the SIR of #8. Findings that need an answer: (1) the sample writes through `#T`, which rule 1 of #11 forbids; stage 0 accepts it, and the test copy declares `~u8` and `~Token`, with `fmt_u64` taking `~u8 buf`; (2) `~=` and prefix `~x` are not in the S0 table, though a `~T` pointer cannot be borrowed into without `~=`; stage 0 accepts both as `#=` and `#x`, and accepts `:#T` and `:~T` the same way; (3) S0 has no `free`, but rule 9 of #11 requires every owner to be released, so an S0 program that calls `alloc` cannot satisfy both; (4) S0 has no `null`, and under dereference by default `p == q` compares the pointees; the corpus uses a `has_next` flag; (5) stage 0 rejects a literal with no typed context, so `while (1)` and `1 == 1` do not compile, and the corpus uses `const u8 TRUE = 1;`; a literal next to a typed operand takes that operand's type; (6) `write` is POSIX, and the corpus uses ISO C `putchar`. Stage 0 also rejects, because the design is open: a literal with no typed context, unary minus, literal forms other than decimal, a local that reuses a name in scope, `null`, a name that is a keyword of full Cesium, and assigning a struct or array as a whole. Open question 4 answered as above. The full list of choices made without guidance is in #60 and `bootstrap/README.md`.
- **2026-11-17: correction to finding 4, and stage 1 (#62).** S0 can test a pointer: `(#p[0]) as uword` is the pointer's own value as a number; whether that spelling is intended is for #26 and #43. `cesium1` is a complete compiler for S0, written in S0 (about 3,400 lines) and built by `cesium0`; compiling its own source it produces the same core SIR as `cesium0`, and the second generation reproduces it. Writing it showed: no struct assignment or struct returns, one namespace for the whole unit, no initialized arrays, no way to name `stderr`, and a trailing `return` needed after `exit()`.
- **2026-11-23: direction changed (emanspeaks, 2026-11-22).** S0 gets `import` of Cesium modules, and a build script gives the compiler its entry point; this came up in cesiumlang/vscode-cesium#7. One file is the entry point, and the dependency graph follows its imports; nested directories and relative paths work; nothing scans a folder. The compiler reads a build script written in Cesium, `build.csm` by default, which sets the entry point with a built-in such as `set_entrypoint()`, reserved for build scripts the compiler reads directly. The entry function is assumed to be `main`. The compiler runs the script JIT while building; a language server can evaluate it without executing the build. Compiler settings go in a JSONC file in the working directory or repository root. A direction, not a decision.

Directions of emanspeaks from other issues that change S0. Each is a direction, not a decision:

- **2026-11-22/23, #9, #19, #47:** `import` of Cesium modules, and the build script `build.csm` that sets the entry point. Changes the "Compilation unit" row, removes modules and `import` from point 16, and moves `namespace` and `import` from the S1 candidates of point 26 into S0.
- **#19:** the S0 import form is `import(ns, "path/to/file.csm")`, relative to the importing file or absolute, the namespace defined in exactly one file, everything statically linked; imports may form cycles; an imported file exposes only what it marks public. Library names are in double quotes. Changes the "Outside world" row and the `import('c')` and `import('csir')` of the sample and of point 23.
- **#34:** numeric conversion is written `u64(x)`, and `as` is bit reinterpretation or a pointer cast. Changes "casts `as`" in the "Expressions" row, the `e as T` row of the mapping, and the sample's `as u8` and `as u64`.
- **#39:** a string literal is a static `str` that converts to `#u8`. Keeps the S0 use of a literal as a `#u8`, while `str` stays out of S0 (point 16).
- **#38:** `true` and `false` are `u8` constants; a condition may be an integer or a pointer (non-null is true). Replaces the corpus's `const u8 TRUE = 1;`.
- **#33:** a literal with no context is `i32`, else `i64`. Answers part of open question 2 and finding 5, which stage 0 rejects today.
