# Stage 0

The root of the Cesium bootstrap chain: a small compiler in portable C for S0, a tiny subset of Cesium. Its one job is to compile stage 1 (`cesium1`), which is written in S0. The design is recorded in these issues, and this directory implements them:

| Record | Covers |
| --- | --- |
| [cesiumlang/cesium#9](https://github.com/cesiumlang/cesium/issues/9) | Stage 0, the S0 subset, the S0 to core MIR mapping |
| [cesiumlang/cesium#8](https://github.com/cesiumlang/cesium/issues/8) | Core MIR and its lowering to QBE IL |
| [cesiumlang/cesium#10](https://github.com/cesiumlang/cesium/issues/10) | The C dialect stage 0 is written in |
| [cesiumlang/cesium#7](https://github.com/cesiumlang/cesium/issues/7) | ADR 0004: QBE first, LLVM later |
| [cesiumlang/cesium#11](https://github.com/cesiumlang/cesium/issues/11) | Ownership: the pointer forms S0 source uses |

The grammar stage 0 accepts is the S0 chapter of the draft specification on cesiumlang.org (`spec/s0-grammar`), which transcribes #9.

## Layout

| Path | Holds |
| --- | --- |
| `include/` | `cs_common.h` (arena, buffer, checked sizes) and `cs_banned.h` (the banned-functions header) |
| `libcsmir/` | Core MIR: data structures, text parser and printer, verifier, layout, QBE lowering, and the coarse C API (`csmir.h`) |
| `stage0/` | `cesium0`: the S0 lexer, parser and checker, and the driver |
| `runtime/` | `cs_arena_alloc`, the bump arena behind S0's `alloc(count)` |
| `stage1/` | The beginning of `cesium1`, written in S0: so far its lexer and a driver that prints tokens |
| `tests/` | The test corpus |

## Building

Stage 0 is built with the Zig of the [Cesium devkit](https://github.com/cesiumlang/devkits), which also supplies QBE as `libqbe.a`. Unpack the devkit for your host to `.devkit` in the repository root (or pass `-Ddevkit=<dir>`):

```sh
python tools/fetch-devkit.py        # downloads and checks the devkit named in devkit-version
.devkit/zig/zig build               # cesium0, libcsmir, libcsrt in zig-out
.devkit/zig/zig build test          # builds as C99 and as C11, then runs the corpus
```

## Using `cesium0`

```sh
cesium0 -o out.s a.csm b.csm          # S0 files, compiled as one unit, to assembly
cesium0 --emit-mir -o out.mir a.csm   # the core MIR text instead
cesium0 --emit-qbe -o out.ssa a.csm   # the QBE IL instead
cesium0 -o out.s --mir in.mir         # core MIR text to assembly
```

`--target` takes a QBE target name (`amd64_sysv`, `amd64_apple`, `amd64_win`, `arm64`, `arm64_apple`, `rv64`) or a triple starting with `<arch>-<os>`; the default is the machine `cesium0` runs on. Stage 0 does not spawn processes: the build system assembles and links, with the devkit's `zig cc`, adding `libcsrt` for `alloc`.

## The pipeline

S0 source → lexer → parser → checker → core MIR in memory → `libcsmir`: verify → QBE IL → QBE, run in-process → assembly.

MIR text takes the same path from the verifier on. `cesium1` will emit MIR text and call `cs_compile_mir`.

## What stage 0 enforces

Stage 0 does the minimal checking that keeps S0 source meaningful:

- **No implicit conversions.** Operands of a binary operator have one type; `as` converts.
- **Integer literals** take the type their context gives: the declared type, a parameter type, or the other operand's type.
- **Pointers** are dereferenced by default. A pointer is used as a pointer only where one is expected (an argument for a pointer parameter, the right side of `:=`, `#=` or `~=`, a returned pointer, the operand of a cast to a pointer type); everywhere else it is read through. `p[i]` and `p.f` reach through it.
- **Plain `=`** assigns through a pointer and never initializes one. `:=`, `#=` and `~=` set where a pointer points, and only a pointer takes them.
- **A plain value** passed where a pointer is expected has its address taken with `#x` or `~x`.
- **`alloc(count)`** appears only on the right of `:=`; its element type comes from the pointer.

Stage 0 does not check ownership or borrowing, and treats `~T` exactly as `#T` (#9).

## Open questions stage 0 refuses to answer

S0 source is frozen once stage 1 works, and every later compiler must still accept it with the same meaning. So where the language design has no recorded answer, stage 0 rejects the program instead of choosing one:

| Rejected | Why | Open in |
| --- | --- | --- |
| A literal whose type no context gives, such as `if (1)` or `1 == 1` | The default type of an integer literal is undecided | #9, open question 2 |
| Unary minus | Not in S0's operator list; no place in the precedence table | [#15](https://github.com/cesiumlang/cesium/issues/15) |
| Hexadecimal, binary and other literal forms | Only decimal literals are specified | [#56](https://github.com/cesiumlang/cesium/issues/56) |
| A local that reuses a name already in scope | Whether shadowing is allowed is not recorded | none yet |
| `null` | Not in S0's list; how a pointer is tested or compared under dereference-by-default is undefined, so S0 has no way to compare two pointers (`p == q` compares what they point at) | [#26](https://github.com/cesiumlang/cesium/issues/26), [#43](https://github.com/cesiumlang/cesium/issues/43) |
| A name that is a keyword of full Cesium | S0 must stay a subset; the keyword list is the reference's | [#50](https://github.com/cesiumlang/cesium/issues/50) |
| Assigning or initializing a struct or array as a whole | What `=` does to an aggregate is undecided | [#22](https://github.com/cesiumlang/cesium/issues/22) |

## Choices made without guidance

These had to be settled to write stage 0, and no issue decides them. Each is isolated so it can change; they are listed for review in the pull request that added this directory.

**S0 frontend**

- **`!` and `as` precedence.** `!` is a prefix operator at the level of address-of; `as` binds tighter than every binary operator. This is the provisional placement of the draft specification (#15). S0 source should parenthesize the operands of both.
- **`~=`, prefix `~x` and `:#T` / `:~T`** are accepted and treated as `#=`, `#x` and the plain pointer type. #9 lists only `~T`; #11 says stage 0 parses the ownership forms without enforcing them.
- **Escapes** in character and string literals: `\n`, `\t`, `\r`, `\0`, `\\`, `\'`, `\"`. The set is undecided ([#39](https://github.com/cesiumlang/cesium/issues/39)); #9's sample uses `\n`.
- **A literal operand takes the other operand's type** (`n + 1`, `v % 10`). #9 leaves open only the case where no context exists.
- **An index** may have any integer type; a literal index is a `uword`.
- **`extern` functions are called by their bare name**, as in #9's sample. The block's name is parsed and unused ([#19](https://github.com/cesiumlang/cesium/issues/19)).
- **Only `main` is exported**; every other function is internal to the unit.
- **Array lengths** are integer literals. **Constants** are integers initialized by a literal or another constant. **Global initializers** are a literal or constant for a number, a string for a `#u8`.
- **An expression statement** is a call.
- **Struct and array locals and globals** are allowed, though #9 lists only fixed arrays explicitly.
- **Tests use ISO C `putchar`**, not the POSIX `write` of #9's sample, so they run on Windows.

**Core MIR and the QBE lowering**

- **Text syntax** follows the sketch in #8. Additions: `else` after `if`; a string escapes bytes as `\HH`; a global initializer is a typed constant, a string, or `@name`; names may contain `.`.
- **Source locations** (`loc`) are not implemented; QBE output carries no line information yet.
- **Division or remainder by zero, and signed `MIN / -1`, trap** (QBE `hlt`). #8 requires one defined behavior and gives a trap as its example; which one is its open question 3.
- **Shift amounts** are reduced modulo the operand's width, the rule #8 names.
- **A local without an initializer starts as all zero bits**, so that no MIR program reads an undefined value. #8 requires defined semantics but does not mention this case.
- **Locals are block-scoped**, and an inner block may reuse a name; the S0 frontend never does.
- **Layout** is the C layout of the QBE targets (all LP64-style), computed in `mir.c`. The layout-query interface of #8 does not exist yet.
- **Read-only data** is not placed in a read-only section yet: `const` and `global` lower the same way.

**C dialect**

- **`-Wformat-nonliteral` and the ban on attributes conflict** for the project's own printf-style functions. All forwarding of format strings is confined to `libcsmir/cs_format.c`, the one file built without that warning, and `-Wno-missing-format-attribute` is added everywhere. This follows #10's rule for third-party headers: relax one translation unit, not the rules.
- **Not yet enforced** from #10: the source check for banned identifiers, the GCC, MSVC and TinyCC builds, ASan, and fuzzing. Debug builds do run under Zig's default UBSan.

**In-process QBE**

- QBE is run through the devkit's `libqbe.a`, which is QBE's own `main` renamed to `qbe_main`. Like the program, it ends the process when it is done, so `cs_compile_mir` does not return on success. That is enough for `cesium0` and for a `cesium1` that compiles and exits; an entry point that returns needs a change in the devkit.

## Stage 1

`stage1/` is the start of `cesium1`, the compiler written in S0 that stage 0 exists to build (#9). It has a lexer for the S0 token set, which every later language level contains, and a driver that reads source on standard input and prints one line per token. `zig build test` compiles it with `cesium0`, links it, and checks its output on two source files, one of them its own lexer.

What S1 adds to S0 is not decided ("candidate S1 features, to be confirmed when designing stage 1", #9), so nothing beyond S0's tokens is in it. Writing it showed that S0 as implemented is enough for this kind of code. It reads input with `getchar`, which returns an `i32`: S0 cannot test a pointer, so functions that report failure through a null pointer are out of reach.

## Tests

`zig build test` runs `tests/` (see `build-src/tests.zig`):

- `s0/<name>.csm` with `<name>.expected`: compiled, assembled and linked with `zig cc`, run, and the output compared. `tokens.csm` is the sample of #9 with its amendment applied; its core MIR is compared with `tokens.mir`.
- `s0/errors/`: programs stage 0 must reject, each with the message it must give.
- `mir/<name>.mir` with `<name>.expected`: MIR text printed back unchanged, then compiled and run. `integers.mir` covers the sub-word integer types S0 does not have; `layout.mir` checks sizes, alignments and field offsets.
- `mir/errors/`: MIR the verifier or parser must reject.
- `stage1/<name>.tok`: the tokens `cesium1` must print for a source file.
