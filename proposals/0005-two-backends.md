+++
id = "P-0005"
type = "proposal"
status = "proposed"
+++

# Proposal P-0005: two backends, QBE first and LLVM as the primary release backend

This proposal sets the two backends behind the middle IR (MIR), the order in which they appear, how they are kept consistent and what each can do. It carries the decision record cesiumlang/cesium#7 (dated 2026-11-16, moved to GitHub the same day), which marked itself "Accepted (planning; implementation not started)" but was guidance written by an agent and was not accepted. It relates to #4 (LLVM as the primary release backend, unchanged; P-0002), #5 (bootstrap, whose order of backends in the chain it changes; P-0003), #6 (freestanding; P-0004), #8 (middle IR) and #9 (stage 0 and S0). emanspeaks accepts, changes or marks open each numbered point below; nothing in it is decided until he accepts it.

## Motivation

The record names these gains:

- Writing stage 0 can start immediately: no LLVM build, devkit fork or `libcsllvm` is on its critical path.
- Edit-compile cycles are fast.
- A second, independent backend makes differential testing possible, which catches bugs in MIR and in both lowerings.
- There is a quick path to new architectures.

## Design

1. **Two backends behind MIR:**

   | Backend | Role | Strengths |
   | --- | --- | --- |
   | QBE (MIT, vendored and pinned) | Development, bootstrap and porting backend; correctness first | Builds in seconds with any C compiler; very fast compiles; correct C ABI including structs by value; a port to a new architecture is roughly 1.5k to 2k lines |
   | LLVM (through `libcsllvm`) | Primary release backend (#4) | Optimization and vectorization, every Cesium target, full debug info (DWARF, CodeView/PDB), freestanding features |

2. **MIR is the contract.** Both backends consume the same core MIR; the numerics level lowers into core MIR (plus hints that only LLVM uses) before either backend sees it.
3. **Cesium targets QBE first and later bootstraps up to LLVM.**
4. **Phase 1: stage 0 (portable C) and `cesium1` use QBE.** `build.zig` assembles and links QBE's output (assembly) with the pinned `zig cc` in CI. No LLVM build, devkit fork or `libcsllvm` is on the critical path to start writing stage 0.
5. **Phase 2: the real compiler starts with the QBE backend too.**
6. **Phase 3: the LLVM backend and `libcsllvm` are added** when numerics performance, more targets, full debugging or freestanding features matter.
7. **Phase 4: from then on, releases use LLVM.** QBE remains for development iteration, the bootstrap chain and quick ports. `cesium1` and the real compiler can then be self-contained: they assemble QBE output through `libcsllvm`, using LLVM's integrated assembler (`module asm` wrapping), and link with in-process LLD (tested 2026-11-16).
8. **Each backend owns the target-specific lowering,** so LLVM can do what it does best:
   - LLVM backend: struct layout and `sizeof`, `alignof` and offsets from LLVM's data layout, `getelementptr` address arithmetic, type legalization, C ABI attributes, debug-info emission.
   - QBE backend wrapper: the equivalent, implemented itself: C layout rules per target, explicit address arithmetic for `index` and `field`, extension and truncation for 8- and 16-bit integers (QBE computes in 32 or 64 bits and uses sized loads and stores), constant folding of `sizeof` and `alignof`, struct ABI through QBE aggregate types, structured control flow lowered to labels and branches.
9. **Layout stays symbolic in MIR.** MIR keeps `sizeof(T)`, `alignof(T)`, `index` and `field` symbolic; frontends never compute layout. When a frontend needs a layout value at compile time (constant expressions, static assertions, later `comptime`), it asks the active backend through a layout-query interface: for LLVM, the target-data queries of the C API such as ABI size, ABI alignment and element offset; for QBE, the wrapper's own layout code.
10. **Consistency between the backends is proven by differential testing, not by construction.**
11. **Core MIR has no undefined behavior.** Differential testing is only reliable if both lowerings mean exactly the same thing, so core MIR operations have fully defined semantics:
    - integer `add`, `sub`, `mul` and `neg` wrap (two's complement);
    - shift amounts are reduced modulo the operand width, or follow another single defined rule;
    - division and remainder by zero, and signed `MIN / -1`, have one defined behavior in both lowerings (for example a trap through an explicit check);
    - out-of-range conversions from float to integer have one defined result.
12. **The LLVM lowering adds no poison- or UB-producing flags** (`nsw`, `nuw`, `exact`, `inbounds` where not proven, `nnan`, `ninf` or fast-math) unless MIR semantics or a verified fact allows it, such as a fact from the borrow checker or the floating-point policy of the numerics level.
13. **The capabilities differ:**

    | Feature | QBE backend | LLVM backend |
    | --- | --- | --- |
    | Targets | QBE's list (amd64 SysV, Apple and Windows; arm64 Linux and Apple; rv64) | All Cesium targets (#4) |
    | SIMD and vector operations | Scalarized by a MIR legalization pass (also needed for targets without SIMD, such as PowerPC 750) | Native vector types |
    | Optimization | QBE's modest optimizations | The full pipeline |
    | Debug info | Line tables only (`.file`, `.loc`) | Full variables and types: DWARF; CodeView/PDB on Windows |
    | Freestanding attributes (interrupt calling conventions, naked functions, sections, no red zone, stack probes) | Not available: building a kernel requires LLVM (#6) | Available |
    | Output | Assembly text (needs an assembler and a linker) | Object files (in-process LLD links) |

14. **Code that needs a capability a backend lacks** is legalized (for example scalarized) or rejected with a clear error that names the backend.
15. **Differential testing.** Every test program compiles through both backends on each CI host QBE supports, and the outputs must match exactly:
    1. Corpus tests: the test corpora of stage 0, S1 and the compiler through QBE, LLVM -O0 and LLVM -O2.
    2. Layout tests: a generated catalog of structs, arrays and nested aggregates; each backend's build prints `sizeof`, `alignof` and every field offset. A third oracle is the equivalent C declaration compiled by `zig cc`, which is the ground truth of the C ABI.
    3. ABI tests: generated calls between Cesium (built with each backend) and C (built with `zig cc`) in both directions, with mixes of integer, float, pointer and struct-by-value arguments and returns, in the spirit of Zig's C ABI tests.
    4. Random programs: once MIR stabilizes, a Csmith-style generator of well-defined MIR programs, with a reducer to shrink failures.
    5. Floating point: exact bit comparison wherever the floating-point policy is strict (consistent with rstd's bit-reproducibility rules); relaxed policies are compared within documented bounds.
16. **On targets QBE does not support, only LLVM runs,** checked against the same expected outputs.
17. **QBE is vendored, pinned and treated as third-party code,** exempt from the stage 0 C dialect and banned-functions rules (#10).
18. **QBE is called in-process.** Stage 0 and `libcsmir` call it with its entry point renamed from `main`, writing assembly to a path the build system supplies; stage 0 does not spawn processes.
19. **Porting to an architecture LLVM does not support** takes a QBE port plus an assembler and a linker for that platform (typically GNU binutils).
20. **Performance benchmarks always run on LLVM,** because QBE's code quality says little about Cesium's numerical performance.

## Alternatives

The record gives none.

## Drawbacks

- Two lowerings to maintain.
- A larger test matrix.
- Every new MIR construct needs both lowerings, or a legalization.
- QBE's code quality says little about Cesium's numerical performance (point 20).

## Open questions

1. The single defined rule for shift amounts: reduction modulo the operand width is the record's example, not a settled choice.
2. The one defined behavior of division and remainder by zero and of signed `MIN / -1`: a trap through an explicit check is the record's example, not a settled choice.
3. The one defined result of out-of-range conversions from float to integer: the record does not say which.
4. The documented bounds within which relaxed floating-point policies are compared.

## What changed since the record

Nothing. The issue has no comments.
