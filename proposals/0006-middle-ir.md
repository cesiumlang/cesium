+++
id = "P-0006"
type = "proposal"
status = "proposed"
+++

# Proposal P-0006: the Cesium middle IR (MIR)

This proposal decides the role and the core level of Cesium's middle IR (MIR): the two levels, who implements MIR, its defined semantics, the core level's text form, types, layout, declarations, statements and expressions, and how core MIR lowers to QBE and to LLVM. It carries the decision record cesiumlang/cesium#8, which was guidance and not accepted; the record calls itself a draft sketch whose syntax is illustrative and open to change. emanspeaks accepts, changes or marks open each numbered point. Nothing in it is decided until he accepts it.

## Motivation

The middle IR is the core design asset of cesiumlang/cesium#4 (ADR 0001). Every frontend lowers to it, and it lowers to the two backends of #7 (ADR 0004), QBE first and LLVM as the primary release backend. Both backends have to give a program the same meaning, or differential testing between them cannot be trusted. The structure is the same as Rust's MIR, Swift's SIL or LFortran's ASR.

Structured control flow at the core level also suits SPIR-V, which requires structured control flow, and WebAssembly. A direct SPIR-V emitter, the fallback in #4, would start from MIR.

The record is related to #4 (middle IR as the core design asset), #5 (bootstrap), #6 (freestanding attributes), #7 (two backends) and #9 (stage 0 and S0, P-0007).

## Design

### Role and levels

1. Every Cesium frontend lowers to MIR; MIR lowers to a backend: QBE (development, bootstrap, porting) or LLVM (primary release backend), per #7.
2. Language features are defined by how they lower. Core features map directly onto core MIR constructs. Composite features are rewrites into the core: `for` to `while`, `switch` to `if` chains or jump tables, `defer` to cleanup blocks, error unions to tagged structs, methods to functions taking `this`, and so on.
3. MIR has two levels. The core level comes now: close to "C with structured control flow" (scalars, pointers, structs, arrays, calls, structured `if` and `while`, named mutable locals). It is not SSA: locals are stack slots (LLVM `alloca`s, which `mem2reg` promotes; QBE `alloc` slots, which QBE promotes). Stage 0 implements only this level.
4. The numerics level comes later, designed with frontend experience: shapes and array descriptors, alias facts from the borrow checker, vector types and FMA, loop nests over index spaces, fusion regions, per-operation floating-point policy. It lowers into the core level plus hints that only LLVM uses (`noalias`, fast-math flags, native vector types). For QBE and for targets without SIMD, a legalization pass scalarizes vector operations.
5. The topics of the numerics level, to design later: vector types (`vec<N x T>`) and an FMA operation, 128-bit only on Wasm, with a scalarization pass for QBE and non-SIMD targets; array descriptors and shapes, strides and contiguity; alias and region facts from the borrow checker, lowered to `noalias` and scoped alias metadata (LLVM only; ignored by QBE); loop nests over index spaces, parallel loops, fusion regions, tiling; per-operation floating-point policy (contraction, reassociation), consistent with rstd's bit-reproducibility rules and with differential testing; library dispatch points (BLAS-style kernels).

### Who implements what

6. `libcsmir` is written in portable C (part of stage 0, compiled by `zig cc`): the core MIR text parser, verifier and printer; the MIR to QBE IL lowering (first); the MIR to LLVM IR lowering (added when the LLVM backend arrives); vendored QBE. It is used by stage 0 and linked into `cesium1`, and frozen with the bootstrap chain.
7. The real compiler's MIR (both levels) and both lowerings are written in Cesium and replace `libcsmir` in the real compiler. Because frozen layers keep their own `libcsmir`, the real compiler can evolve MIR freely: only `libcsmir` and the frontend that feeds it must agree on a given version.

### Semantics: no undefined behavior in core MIR

8. Both backends give every core MIR program the same meaning; core MIR has no undefined behavior.
9. Integer `add`, `sub`, `mul` and `neg` wrap (two's complement).
10. Shift amounts are reduced modulo the operand width, or another single defined rule, applied by both lowerings.
11. Division and remainder by zero, and signed `MIN / -1`, have one defined behavior in both lowerings (for example a trap via an explicit check).
12. Out-of-range float to integer conversions have one defined result.
13. The LLVM lowering does not add poison- or UB-producing flags (`nsw`, `nuw`, `exact`, unproven `inbounds`, fast-math) unless MIR semantics or a verified fact allows it.

### The core level

14. Lexical conventions: `@name` for globals and functions, `%name` for parameters and locals, bare names for types and operations. Constants carry their type: `0:u8`, `1:uword`, `-5:i64`. `;` starts a comment. The file starts with a version header, `cesium-mir 0`.
15. Types: the integers `u8 u16 u32 u64 i8 i16 i32 i64` and `uword` (pointer-sized, resolved by the target), with signedness in the type, so operations need no signed and unsigned variants; the floats `f32 f64` (in the core level even though S0 does not use them); pointers `ptr<T>`, typed in MIR and erased by the lowerings, the typing kept for the numerics level's alias facts; arrays `[N x T]`; structs `struct Name { field: T, ... }`; `void`.
16. Layout is owned by the backend: frontends never compute layout. MIR keeps `sizeof(T)`, `alignof(T)`, `index` and `field` symbolic, and each backend resolves them for the target: LLVM through its data layout and `getelementptr`; QBE through the wrapper, which computes C layout rules per target and emits explicit address arithmetic.
17. When a frontend needs a layout value at compile time (constant expressions, static assertions, later `comptime`), it calls a layout-query interface provided by the active backend (LLVM: the target-data queries of the C API for ABI size, ABI alignment and element offset; QBE: the wrapper's layout code). Agreement between the backends, and with the C ABI, is checked by differential layout tests against `zig cc` as the C ground truth (#7).
18. Declarations, as sketched below: `func` is module-internal; `export func` is externally visible with the C ABI; `extern func` declares an external C ABI function (parameter types only). A global name used as an expression yields its address (`ptr<T>`).
19. Function attributes are reserved for #6: `callconv(...)`, `section(...)`, `align(...)`, `target_features(...)`, `noredzone`, `naked`, `probe_stack(...)`. v0 implements `callconv(c)` only. Freestanding attributes are LLVM-only; the QBE lowering rejects them with a clear error.
20. The statements are those of the table below.
21. Any statement may carry a source location, `loc "file" line:col` (syntax to be settled), used for debug info.
22. Expressions are trees, not three-address code; the lowerings flatten them. The expressions are those of the table below. Comparisons and `land`, `lor` and `lnot` give a `u8`, since Cesium has no `bool`.
23. `as(T, v)` converts with fixed rules: integer widening by source signedness, narrowing truncates, int to float and back, pointer to pointer, pointer to and from `uword`.
24. Every expression's type is determined bottom-up. The verifier checks types, that `break` and `continue` are inside loops, and that non-`void` functions return on every path.
25. v0 restriction: parameters, return values and `extern` and `export` signatures use only scalars and pointers. Struct-by-value arguments and returns come later: QBE handles them through aggregate types; the LLVM lowering needs per-target ABI classification (`byval`, `sret`, coercion), checked by the differential ABI tests.
26. v0 restriction: no floating-point operations beyond `as` conversions until a frontend needs them; the types are reserved.

Declarations, as the record sketches them:

```text
cesium-mir 0

struct Token { kind: i64, start: ptr<u8>, len: uword }

extern func @write(i32, ptr<u8>, uword) -> i64 callconv(c)

const @str.0: [6 x u8] = "hello\00"
global @counter: i64 = 0:i64

export func @main(%argc: i32, %argv: ptr<ptr<u8>>) -> i32 { ... }
func @cstr_len(%s: ptr<u8>) -> uword { ... }
```

Statements:

| Statement | Meaning |
| --- | --- |
| `local %x: T [= expr]` | Mutable local (stack slot) |
| `set %x = expr` | Assign a local |
| `store ptr_expr, value_expr` | Store through a pointer |
| `eval expr` | Evaluate for side effects (a call, for example) |
| `if cond { ... } [else { ... }]` | Structured conditional; `cond` is an integer, nonzero is true |
| `while cond { ... }` | Structured loop |
| `break` / `continue` | Innermost loop |
| `return [expr]` | Return |

Expressions:

| Expression | Meaning |
| --- | --- |
| `%x` | Value of a local or parameter |
| `addr(%x)` | Address of a local |
| `load(p)` | Load through `p: ptr<T>`, yields `T` |
| `index(p, i)` | `ptr<T>` to element `i` (scaled by `sizeof(T)`) |
| `field(p, name)` | `ptr<F>` to field `name` of the struct `p` points to |
| `call @f(args...)` | Call; usable as an expression |
| `add sub mul div rem` | Arithmetic (`div` and `rem` signedness from the operand type; semantics above) |
| `and or xor shl shr not neg` | Bitwise (`shr` arithmetic or logical from the operand type; shift semantics above) |
| `eq ne lt le gt ge` | Comparisons; result type `u8` (Cesium has no `bool`) |
| `land(a, b)`, `lor(a, b)`, `lnot(a)` | Short-circuit logical operations, result `u8` |
| `as(T, v)` | Conversion with the fixed rules of point 23 |
| `sizeof(T)`, `alignof(T)` | Target-dependent, resolved by the backend, result `uword` |

The record's example: the S0 function

```cesium
uword = cstr_len(#u8 s) {
    uword n = 0;
    while (s[n] != 0) {
        n = n + 1;
    }
    return n;
}
```

lowers to

```text
func @cstr_len(%s: ptr<u8>) -> uword {
  local %n: uword = 0:uword
  while ne(load(index(%s, %n)), 0:u8) {
    set %n = add(%n, 1:uword)
  }
  return %n
}
```

### Lowering to QBE IL (first backend)

27. Locals become QBE `alloc` stack slots, which QBE promotes to registers. Structured control flow becomes labels and conditional jumps.
28. 8- and 16-bit integers: QBE computes in 32- and 64-bit temporaries, so the lowering uses sized loads and stores and explicit extension and truncation.
29. `index` and `field` become explicit address arithmetic using the wrapper's layout; `sizeof` and `alignof` become constants. Comparisons become QBE comparisons (32-bit results) narrowed to `u8`. Strings and globals become QBE data definitions.
30. Debug: source locations become QBE line directives (`dbgfile` and `dbgloc`, available in recent QBE versions; to verify against the vendored version), giving line tables only.
31. Output: assembly text, assembled and linked by `zig cc` in CI (phase 1), or by `libcsllvm` (`module asm` and LLD) later (#7).

### Lowering to LLVM IR (primary release backend)

32. Locals become `alloca`s in the entry block; LLVM `mem2reg` produces SSA. Structured control flow becomes basic blocks and branches.
33. `ptr<T>` becomes the opaque `ptr`; `index` and `field` become `getelementptr` with layouts from LLVM's data layout. Comparisons become `icmp` or `fcmp` and a `zext` to `i8`; signedness chooses `sdiv` or `udiv`, `ashr` or `lshr`, `slt` or `ult`, and so on.
34. Debug info: source locations and local and type information become LLVM debug metadata (`DICompileUnit`, `DISubprogram`, `DILocalVariable`, `DILocation`, basic, derived and composite types). LLVM emits DWARF on ELF and Mach-O, and CodeView (`.debug$S` and `.debug$T`, PDB through `lld-link`) on Windows. The record verified this on 2026-11-16 for textual IR: DWARF variable entries with frame locations on Linux, CodeView `S_LOCAL` records on Windows (object level).
35. Output: textual LLVM IR passed to `libcsllvm`'s `cs_compile_ir`.

## Alternatives

The record gives none.

## Drawbacks

The record names none.

## Open questions

As the record lists them; the first implementation's handling of 1, 3 and 4 is under "What changed since the record".

1. Text syntax details: sigils, statement and expression boundaries, constant syntax, `loc` annotations.
2. Whether the real compiler needs an SSA form internally for its own analyses (the core level is deliberately not SSA).
3. The exact defined semantics for division by zero and over-wide shifts (trap or defined value), and their cost in the LLVM lowering.
4. The shape of the layout-query interface between frontends and backends.

## What changed since the record

**2026-11-17: core MIR v0 first implementation (#60).** `libcsmir` implements the core level as sketched: the text parser and printer, the verifier, C layout for the QBE targets, and the lowering to QBE IL. The `cstr_len` example parses, prints back unchanged, and runs. The comment says that none of the following is a decision; each is what the first implementation does, listed to be confirmed or changed.

- Open question 1 (text syntax): as sketched, with these additions: `else { ... }` after an `if` block; byte escapes `\HH` in strings (the sketch shows `\00`), plus `\\` and `\"`; a global's initializer is a typed constant, a string, or `@name` (the address of another global); names may contain `.` (`@str.0`). `loc` annotations are not implemented, so no line information reaches QBE yet.
- Open question 3 (division and shifts): division or remainder by zero traps, and so does signed `MIN / -1`, at every width; shift amounts are reduced modulo the operand's width (the examples of points 10 and 11). The cost in the QBE lowering is one compare and branch per division (two for signed) and one `and` per shift.
- Open question 4 (layout): `sizeof`, `alignof`, `index` and `field` stay symbolic in MIR and are resolved in the QBE lowering with C layout rules. All QBE targets are LP64-style, so there is one rule set. There is no layout-query interface for frontends yet; the S0 frontend never needs a layout value.

Chosen there where the record says nothing:

- A local declared without an initializer starts as all zero bits; otherwise a MIR program could read an undefined value, which point 8 rules out. Aggregates are zeroed with a store loop.
- Locals are block-scoped, and an inner block may reuse a name.
- Typing is strict: both operands of an arithmetic, bitwise or shift operation have the same integer type; comparisons take two integers or two pointers of the same type; `land`, `lor` and `lnot` take integers of any type; a condition is any integer.
- `as` converts integer to integer (widening by the source's signedness, narrowing truncates), pointer to pointer, and pointer to and from `uword`, as point 23 lists. A null pointer is `as(ptr<T>, 0:uword)`.
- A function name is not a value in v0 (no function pointers).
- Sub-word values (`u8`, `i8`, `u16`, `i16`) are kept extended by their signedness in 32-bit QBE temporaries after every operation, and cross calls as QBE's `ub`, `sb`, `uh`, `sh`.
- `const` and `global` lower the same way; read-only data is not in a read-only section yet.
- Only the QBE lowering exists. The floating-point types parse, but every use is rejected, as point 26 says.

One limit of the in-process QBE: `libqbe.a` is QBE's own `main` renamed, so it ends the process when the assembly is written, and `cs_compile_mir` does not return on success. That suits `cesium0` and a `cesium1` that compiles one unit and exits.
