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
| `stage1/` | `cesium1`, written in S0: a compiler for S0 that emits the same core MIR text as `cesium0` |
| `tests/` | The test corpus |
| `diagnostics.md` | The catalog of the error codes both compilers print |

## Building

Stage 0 is built with the Zig of the [Cesium devkit](https://github.com/cesiumlang/devkits), which also supplies QBE as `libqbe.a`. Unpack the devkit for your host to `.devkit` in the repository root (or pass `-Ddevkit=<dir>`):

```sh
bash tools/fetch-devkit.sh          # downloads and checks the devkit named in devkit-version
.devkit/zig/zig build               # cesium0, cesium1, libcsmir, libcsrt in zig-out
.devkit/zig/zig build test          # builds as C99 and as C11, then runs the corpus
```

## Using `cesium0`

```sh
cesium0 -o out.s a.csm b.csm          # S0 files, compiled as one unit, to assembly
cesium0 --emit-mir -o out.mir a.csm   # the core MIR text instead
cesium0 --emit-qbe -o out.ssa a.csm   # the QBE IL instead
cesium0 -o out.s --mir in.mir         # core MIR text to assembly
```

`-O` takes the build mode, with Zig's names: `debug` (the default), `safe`, `fast` or `small` (cesiumlang/cesium#143). QBE optimizes the same in each, so for now the mode decides only whether variables are kept for debuggers: a debug build keeps them unless `-fno-debug-variables` is given, the others only with `-fdebug-variables` (direction of emanspeaks, 2026-11-26). `zig build` passes its own `-Doptimize` mode on to the programs it compiles with `cesium0`, `cesium1` among them.

`--target` takes a QBE target name (`amd64_sysv`, `amd64_apple`, `amd64_win`, `arm64`, `arm64_apple`, `rv64`) or a triple starting with `<arch>-<os>`; the default is the machine `cesium0` runs on. Stage 0 does not spawn processes: the build system assembles and links, with the devkit's `zig cc`, adding `libcsrt` for `alloc`.

## Diagnostics

`cesium0` reports every error it finds in S0 source on standard error as `file:line:col: error[E0101]: message`. The code, `E` and four digits, names the message: [`diagnostics.md`](diagnostics.md) lists every code with its message and what it means, and a code is never reused for another message (cesiumlang/cesium#82). `cesium1` gives the same codes and messages. Where a habit from C or another language causes the error, such as `int x`, `&x`, `*p`, `x += 1` or a return type after `->`, the message names the Cesium form, or says that the form is not decided for S0, under a code of its own. The messages of `libcsmir` (its MIR text parser and verifier, `cesium0: error: ...` and `<file>: error: ...`), the I/O messages and the limits of `cesium1` itself (`error: internal: ...`) have no code yet.

## The pipeline

S0 source → lexer → parser → checker → core MIR in memory → `libcsmir`: verify → QBE IL → QBE, run in-process → assembly.

MIR text takes the same path from the verifier on. `cesium1` will emit MIR text and call `cs_compile_mir`.

## Source locations

Every statement `cesium0` builds carries the location of its first token: the file as named on the command line, the line and the column (bytes from 1, a tab counting as one), as #9 asks. In MIR text a location is a line of its own before the statement, `loc "bootstrap/tests/s0/tokens.csm" 18:3`. The first `loc` of a function names its file; a later one names it only when it changes, and `loc 19:3` has the file of the previous `loc` in the same function. Lines and columns are from 1 to 2^29 - 1, QBE's limit; MIR without `loc` is valid, and `cesium1` reading standard input writes none.

The QBE lowering turns them into `dbgfile "file"` before a function whose first location is in another file than the one before, and `dbgloc line, column` at the start of each statement (for `while`, after the loop label, so each test of the condition has its line; a trap block repeats the line of its statement). QBE cannot change files within a function, so a statement located in another file than the function's first gets no line. QBE writes `.file` and `.loc`, and the assembler builds a line table (`.debug_line`) from them, but no compile unit (`.debug_info`), without which debuggers ignore the table.

So for ELF and Mach-O targets `libcsmir` puts QBE's assembly between a label at the start of the text and a DWARF compile unit after it (`mir_dwarf.c`, cesiumlang/cesium#78), when the module has locations; a module without them gets exactly QBE's assembly. The unit is DWARF 4, the version of the line table the devkit's assembler (`zig cc`) writes on both formats, and has one `DW_TAG_compile_unit` with no children: `DW_AT_producer` `libcsmir`, `DW_AT_language` `DW_LANG_C99` (S0 has C's integers, pointers and calling convention, so a debugger's C expressions fit it, and DWARF has no code for Cesium), `DW_AT_name` the file of the module's first `.file`, `DW_AT_comp_dir` `.` (ISO C cannot ask for the working directory, so paths are relative to where the compiler ran), `DW_AT_stmt_list` the start of the line table, and `DW_AT_low_pc` and `DW_AT_high_pc` around the text. The strings are in `.debug_str` (`DW_FORM_strp`), as compilers write them, since Zig's Mach-O linker misreads `DW_FORM_string` when it writes the debug map. ELF objects get `.debug_str`, `.debug_abbrev` and `.debug_info`, the offsets into them and into `.debug_line` relocated labels, as GCC writes them; Mach-O objects get the same sections in the `__DWARF` segment, and the offsets are computed by the assembler (0 for the abbreviations and the line table), as Mach-O debug sections stay in the object. QBE ends the process without closing its output, so `libcsmir` has it write to standard output opened on the output file, and writes the compile unit there at exit (`atexit`).

For COFF targets, whose debuggers read CodeView from a PDB and not DWARF, `libcsmir` translates instead (`mir_codeview.c`, cesiumlang/vscode-cesium#47), keeping QBE as it is: QBE writes to a file beside the output, `out.s.qbe.s`, which at exit becomes the output with each `.file` made `.cv_file`, each `.loc` a `.cv_loc` (of several in a row, the last, which is the line of the code after them), a `.cv_func_id` for each function that has lines and a label at its end. A function's entry gets the line of its first statement, as QBE's first `.loc` comes after the prologue, and a debugger stepping into a function stops at its entry, which would otherwise have no line. Each prologue is described for unwinding too, in `.seh_` directives (`mir_seh.c`): QBE writes no unwind data, without which a Windows debugger takes every function for a leaf and finds no caller, so the call stack ends at the current function and stepping out runs on to the end. As QBE moves `rsp` around each call, a function with a frame pointer is unwound from `rbp`, its frame set after the saved registers; a frame larger than an unwind record's offset (240 bytes) is set right after `movq %rsp, %rbp` instead, and its saved registers are left out of the record. A `.debug$S` section follows, as clang writes it: an `S_COMPILE3` (`libcsmir`, C, x64), a procedure record (`S_GPROC32`) for each such function, then its `.cv_linetable` and the file and string tables the assembler makes from the directives. The devkit's assembler writes the CodeView, and the linker the PDB. The path in `dbgfile` is absolute, with backslashes, without `.` and `..`: the linker would make a relative path absolute with forward slashes, and the Windows debugger engine then matches neither a file name nor a backslash path against it. The current directory comes from the one platform wrapper, `cs_platform.c` (P-0008, point 14). Output to standard output (`-o -`) gets no CodeView, as there is no file to rewrite.

## Variables

A build that keeps variables (`-O debug`, or `-fdebug-variables`) gives debuggers each local and parameter with its type, in its lexical scope (cesiumlang/cesium#143). QBE promotes stack slots to registers and does not say where values go, so such a build keeps every local in memory, as `clang -O0` does, through markers the rewrite of the assembly reads (`mir_debug.c`, `mir_debug_scan.c`):

- **Pinning:** at each function's entry, the lowering stores the address of each local's slot to a symbol of its own, `.Lcs_vF_N` (function F, local N). QBE does not promote a slot whose address escapes, so the local stays at one offset from the frame pointer (`rbp`, `x29`) or, in a function that calls nothing, from the stack pointer; the rewrite reads the offset from the instruction that takes the address (`leaq 28(%rsp), %rax`; on AArch64 `add x2, x29, #40`). Each local costs a store at entry and its memory accesses.
- **Scopes:** a block that declares a local (the body of an `if`, an `else` or a `while`) is a scope, and stores to `.Lcs_oF_K` where it opens and to `.Lcs_cF_K` where it closes, at the end of the block and again where the code after its statement starts, in case the end cannot be reached. QBE keeps stores to globals where they are; the rewrite puts a label at the first open marker and at the first close marker after it, or at the function's end if none is left.
- **CodeView** (`mir_codeview_vars.c`): an `S_REGREL32` for each local, at its offset from `rbp` or `rsp`, inside an `S_BLOCK32` for each scope, nested as the scopes are, within the function's `S_GPROC32`; their types in `.debug$T`, written as bytes since they need no relocation. The integers and pointers to them are CodeView's own types; a struct is declared forward first, so that a pointer to it, even one of its own fields, refers to the declaration, which the debugger resolves by the name.

A block's scope is one range of addresses, so code QBE places elsewhere is outside it: the trap of a division by zero inside a block, which QBE puts after the function's last `ret`, does not see the block's own locals, though a breakpoint or a step inside the block does.

What debuggers see:

- **Linux (ELF):** gdb sets breakpoints at a `.csm` line and reports a trap at its file and line (`gdb -batch -ex run -ex bt`: `main () at bootstrap/tests/s0/lines.csm:12`); CI checks it on x86_64 with `zig build lines`, `readelf --debug-dump=info` and gdb.
- **macOS (Mach-O):** lldb does the same through the debug map, which points at the objects in Zig's cache, so the debug information is there as long as the cache is; CI runs lldb on arm64 when the runner has it. There is no `dsymutil` step.
- **Windows (COFF):** the PDB has the line tables: `cdb`, the console form of WinDbg, sets a breakpoint by file name or absolute path, steps by `.csm` line and reports a trap at `lines.csm @ 12`; CI checks it on x86_64 with `zig build lines` and `cdb`. Visual Studio's debugger, and VS Code's `cppvsdbg`, read the same PDB with an engine of their own, which no script can drive (`vsdbg` takes only sessions VS Code starts). gdb from MSYS2 reads DWARF, which these objects no longer have.

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
| `null` | Not in S0's list; how a pointer is tested or compared under dereference-by-default is undefined, so `p == q` compares what the pointers point at. A pointer's own value can still be had as a number, `(#p[0]) as uword`, which is the only way S0 source can test or compare one | [#26](https://github.com/cesiumlang/cesium/issues/26), [#43](https://github.com/cesiumlang/cesium/issues/43) |
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

- **Nesting limit.** Expressions and statements may nest 128 levels deep; deeper source is rejected with `nested too deeply`. The parser, the lowering and `libcsmir` recurse along the nesting, and without a limit a source file could overflow the stack (found by fuzzing, see below). An operand, the body of an `if` or `while`, and each further `else if` count as one level; so does each further operator of a chain such as `a + b + c`. #9 names no limits; the number is chosen here, like the limit of 64 arguments. `cesium1` has the same limit and message.

**Core MIR and the QBE lowering**

- **Text syntax** follows the sketch in #8. Additions: `else` after `if`; a string escapes bytes as `\HH`; a global initializer is a typed constant, a string, or `@name`; names may contain `.`.
- **Source locations** (`loc`, #8 open question 1) are written as above, under [Source locations](#source-locations): a line of their own, the file named once per function and when it changes, line and column.
- **Division or remainder by zero, and signed `MIN / -1`, trap** (QBE `hlt`). #8 requires one defined behavior and gives a trap as its example; which one is its open question 3.
- **Shift amounts** are reduced modulo the operand's width, the rule #8 names.
- **A local without an initializer starts as all zero bits**, so that no MIR program reads an undefined value. #8 requires defined semantics but does not mention this case.
- **Locals are block-scoped**, and an inner block may reuse a name; the S0 frontend never does.
- **Layout** is the C layout of the QBE targets (all LP64-style), computed in `mir.c`. The layout-query interface of #8 does not exist yet.
- **Nesting limit of MIR text:** types, expressions and statements may nest 1024 levels deep, for the same reason as in the S0 frontend. S0 source at its own limit produces MIR nested about 250 levels deep.
- **Read-only data** is not placed in a read-only section yet: `const` and `global` lower the same way.

**C dialect**

- **`-Wformat-nonliteral` and the ban on attributes conflict** for the project's own printf-style functions. All forwarding of format strings is confined to `libcsmir/cs_format.c`, the one file built without that warning, and `-Wno-missing-format-attribute` is added everywhere. This follows #10's rule for third-party headers: relax one translation unit, not the rules.
- **Enforced in CI beyond `zig build`** (#10, "Enforcement in CI"): `tools/check-c-dialect.sh` is the source check (banned functions, compiler extensions and optional features, headers outside the allowed list, `%p`, and every `.c` file including the common header). `tools/portable-c.sh compile` compiles every translation unit with GCC and Clang under the dialect's flags as C99 and as C11, with TinyCC as the canary, and with MSVC (`/std:c11 /W4 /WX`, minus C4996, MSVC's own deprecation of ISO C functions such as `fopen`). `tools/portable-c.sh sanitize` builds `cesium0` with ASan and UBSan and runs the corpus through it up to QBE IL, with QBE replaced by a stub. A line that must name a banned word carries the comment `cs-dialect: allow` and a reason.
- **Fuzzing** (#10: "consider fuzzing the MIR text parser"): the fuzzer `tools/fuzz-stage0` (Zig, run by `zig build fuzz`) damages the files of the test corpus a little (tokens, lines, bytes, truncation, repetition, deep nesting) and runs `cesium0` on each result, for the MIR text parser and for the S0 front end. Whatever the input, `cesium0` must exit with status 0 or 1, in time and without a sanitizer report, and MIR text it accepts and prints must read back the same. `tools/portable-c.sh fuzz` builds `cesium0` with ASan and UBSan and runs the fuzzer on it; CI does so with a fixed seed, so a failure there can be reproduced. `zig build fuzz -- --iterations 200` fuzzes the build's own `cesium0`, without sanitizers. It is mutation fuzzing without coverage feedback, not libFuzzer or AFL.
- **Not done** from #10: the other compilers compile but do not link: the devkit's `libqbe.a` is built by `zig cc` for its own targets.

**In-process QBE**

- QBE is run through the devkit's `libqbe.a`, which is QBE's own `main` renamed to `qbe_main`. Like the program, it ends the process when it is done, so `cs_compile_mir` does not return on success. That is enough for `cesium0` and for a `cesium1` that compiles and exits; an entry point that returns needs a change in the devkit.

## Stage 1

`stage1/` is `cesium1`, the compiler written in S0 that stage 0 exists to build (#9). It compiles S0, the language it is written in, to exactly the core MIR text `cesium0 --emit-mir` gives for the same unit, byte for byte, and so compiles itself.

```sh
cesium1 a.csm b.csm                  # core MIR text on standard output
cesium1 -o out.s a.csm b.csm         # assembly, through cs_compile_mir (and out.s.ssa)
cesium1 -O fast -o out.s a.csm       # another build mode, as for cesium0
cat a.csm b.csm | cesium1            # the unit on standard input: MIR without source locations
```

`zig build` installs `cesium1` to `zig-out/bin`, next to `cesium0`: `cesium0` compiles `stage1/` and the result is linked with `libcsrt`, `libcsmir` and `libqbe.a` (`build-src/stage1.zig`). It is the same build of `cesium1` the tests check.

The files named are compiled as one unit, each read through C's stdio and parsed in turn with its lines counted from 1, as `cesium0` does; the MIR is that of `cesium0 --emit-mir` for the same files, [source locations](#source-locations) included. With no file named, the unit is read from standard input, where several files are given one after another, as the language server `zcsls` does; its MIR has no `loc` lines and is otherwise the same. The exit status is 0 on success, 1 on an error in the source or a file that cannot be read (`error: cannot read a.csm`), and 2 on a usage error or when a limit of `cesium1` is reached. An error is printed as `a.csm:line:col: error[E0101]: message` on standard output, with the code and message of `cesium0` (see [Diagnostics](#diagnostics)), or `line:col: error[E0101]: message` from standard input, lines counted from its start; nothing else is printed before the whole unit is checked. The two checks of `libcsmir`'s verifier that `cesium1` makes itself print `line:col: error: message`, without a code, like the verifier's own messages. Linking `cesium1` needs `libcsmir` and the devkit's `libqbe.a` besides `libcsrt`.

| File | Holds |
| --- | --- |
| `util.csm` | `libc` declarations, byte buffers, reading standard input, error messages |
| `lex.csm` | Interned names and the lexer |
| `parse.csm` | The syntax tree and the parser |
| `types.csm` | Core MIR types and the tables of structs and functions |
| `habit.csm` | The messages for C type names and `true` and `false`, as `stage0/s0_habit.c` |
| `lower.csm` | Checking expressions and statements, and their MIR text |
| `decl.csm` | Checking declarations, and the text of the unit |
| `loc.csm` | The command line, reading the files, and source locations |
| `main.csm` | The driver |

It follows `stage0/` function for function: the same tokens, grammar, checks and messages, and the same order of declarations, string globals and structs in the MIR. It differs in four ways:

- **MIR is built as text.** An expression's MIR is a fragment of a scratch buffer that the enclosing expression copies into its own; there is no MIR tree to print.
- **The first error ends the program**, through `libc`'s `exit`, so no function passes a failure back up. Stage 0 reports every error it finds.
- **The two checks of `libcsmir`'s verifier that S0 source can fail**, a function that does not return on every path and a struct that contains itself, are made by `cesium1` itself, with the verifier's message.
- **Errors go to standard output**: `stderr` is a C global variable, and S0 can declare only functions from C.

What S1 adds to S0 is not decided ("candidate S1 features, to be confirmed when designing stage 1", #9), so nothing beyond S0 is in it. Writing it showed that S0 as implemented is enough for a compiler. The workarounds it needs:

- **No null.** A link in the syntax tree, or to a type, struct or function, is an index into an array, with 0 for none. A buffer starts with no block, which is never read while its length is 0.
- **No struct assignment, struct results or `realloc`.** Results come back through pointer parameters, structs are copied one field at a time, and a byte buffer grows by allocating a larger block and copying. The arrays of structs are not grown at all: their sizes are bounded by the length of the source (every node has a token of its own), and exceeding one is a limit error.
- **One namespace for every name of the unit**, locals included, and the keywords of full Cesium are not names (no field can be called `type`, `list` or `error`). Every function, global and constant has a prefix per file.
- **No initialized arrays of strings.** Keyword lists are a sequence of calls and token names a chain of `if`.
- **A function that ends in a call that never returns** (`exit`) still needs a `return` after it, and so does one whose last statement is `while (TRUE)`: the verifier counts neither as returning.
- **Text streams.** `cesium1` writes through `putchar`, so on Windows every `'\n'` of its MIR becomes `"\r\n"`; S0 has no portable way to switch standard output to binary. The tests compare with that in mind. `cs_compile_mir` takes the text from memory and is not affected.

## Tests

`zig build test` runs `tests/` (see `build-src/tests.zig`):

- `s0/<name>.csm` with `<name>.expected`: compiled, assembled and linked with `zig cc`, run, and the output compared. `tokens.csm` is the sample of #9 with its amendment applied; its core MIR is compared with `tokens.mir`, made by `cesium0 --emit-mir -o bootstrap/tests/s0/tokens.mir bootstrap/tests/s0/tokens.csm` from the repository root (the test runs it there, so the file named in its `loc` lines is the same on every host).
- `s0/lines.csm`: compiled to assembly by both compilers, which must name its file (`.file 1 "bootstrap/tests/s0/lines.csm"`) and the line of its division by zero (`.loc 1 12 3`). It is not run.
- `s0/errors/`: programs stage 0 must reject, each with the message it must give.
- `mir/<name>.mir` with `<name>.expected`: MIR text printed back unchanged, then compiled and run. `integers.mir` covers the sub-word integer types S0 does not have; `layout.mir` checks sizes, alignments and field offsets; `locs.mir` has source locations, with a file name that needs escapes and a statement in another file than its function's.
- `mir/errors/`: MIR the verifier or parser must reject, malformed `loc` lines among them.
- `cesium1`, built by `cesium0`, on the same corpus: for every `s0/<name>.csm` run test its core MIR for the same files must be what `cesium0 --emit-mir` prints, and it must reject every program in `s0/errors/`, given on standard input, with the same message. On its own source its MIR must be `cesium0`'s too, and the `cesium1` it builds from that source with `cesium1 -o out.s` must print that MIR again: the fixed point. From standard input, its MIR for its own source must be `cesium0`'s without the `loc` lines. `build-src/stage1_tool.c` joins the files of a unit and compares the outputs.
- The catalog of codes, `diagnostics.md`, against the sources (`build-src/diagnostics.zig`): every code (a string literal `"E0101"`) in `stage0/*.c` and `stage1/*.csm` must be in the catalog, and every code of the catalog must be used by both compilers, unless its entry says `cesium0 only` or `cesium1 only`.

`zig build test-diff` runs a second corpus, `diff/`: 246 generated S0 programs (see `diff/README.md`), each given to both compilers (`build-src/tests_diff.zig`). For the 97 in `diff/` the core MIR of `cesium1`, given the program on standard input, must be what `cesium0 --emit-mir` prints without its `loc` lines; for the 149 in `diff/errors/` both must exit with status 1 and their first errors must be the same message at the same line and column (for the two verifier checks `cesium1` makes itself, the same message and line). The step finds the files by listing the two directories, so a case is added by adding its file. It is not part of `zig build test`, which stays fast on every host; CI runs it on Linux x86_64 only (#67, item 3).
