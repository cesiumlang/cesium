+++
id = "P-0010"
type = "proposal"
status = "proposed"
+++

# Proposal P-0010: modules and imports

This proposal decides how a Cesium program is made of files: namespaces, the forms of `import`, what an imported file makes visible, interface files, and the part of all this that S0 has. It carries the design issue #19, with the answers emanspeaks gave there on 2026-11-22 and the rules of the original design that the reference page records. Where emanspeaks left a point to the agent (2026-11-22), the point says it is a recommendation. emanspeaks accepts, changes or marks open each numbered point. Nothing in it is decided until he accepts it.

The build script, which names the entry file and the source roots, is #104's; precompiled Cesium libraries are #107's; the compilation unit, symbol names and generics in interface files stay with #47.

## Motivation

The reference page "Modules and Imports" gives syntax without a model of how files make a program (#47), and its examples disagree with each other and with the design (#19, items 1 to 9). Since the direction of 2026-11-22 (#9), S0 has `import` of Cesium code, so stage 0 (#105), `cesium1` and the language server `zcsls` (cesiumlang/vscode-cesium#7) have to agree on which files make a program and what each file sees.

## Design

Each point names where it comes from: a direction of emanspeaks on #19 (2026-11-22), a rule of the original design of September 2025 as the reference page records it ("Rules from the Original Design"), or a recommendation.

### Namespaces and files

1. A namespace is a name, such as `math.vector`, not a file path. Where its files are is said by an import (point 5) or by the source roots (point 6), never by the name. Original design; confirmed on #19 (direction).
2. A namespace may be defined across several files, as the standard library will be; keeping them together is encouraged, not required. Direction. In S0 a namespace is defined in exactly one file (point 21).
3. A namespace or a sub-namespace can be imported. Original design.

### The forms of `import`

4. `extern name = import("lib") { ... }` declares functions of a C library, always with the C ABI and never with a second argument. The library name is looked up in the system's library path. A dynamic library (DLL, `.so`) comes into a program only this way. Direction.
5. `import(ns, "path")`, with a string, names Cesium code at a path: a source file, a directory of source files, or a compiled Cesium library. A relative path is relative to the importing file. An absolute path is allowed for what is compiled in statically; nothing checks its portability. Source is compiled in, and it is an import error if it does not define `ns`; a compiled Cesium library at the path is linked statically. Direction.
6. `import(ns)`, with one argument, names Cesium source found by searching the source roots recursively for the files that define `ns`. The build script or the compiler's options name the source roots (#104). Direction; not in S0 (#107).
7. `import(ns, name)`, with a bare name, searches the library path for a compiled Cesium library, named without the `lib` prefix and without the `.dll` or `.so` extension, and links it dynamically. Direction; not in S0, and it waits on Cesium's own dynamic library format (#107).
8. Library names are in double quotes, in `import` and in `extern` alike; single quotes are only for characters. Direction (items 5 and 6 of #19).
9. `import(ns) as y` imports `ns` under the name `y`, the form of the reference page; the assignment form `y = import(ns)` of the original design chat is not used. Direction (item 1 of #19).
10. A selective import names the items it takes in braces, separated by `;`, with no `;` after the last item and none after the closing brace; each item may be renamed with `as`: `import(math.matrix) { Matrix; multiply; invert as inverse }`. Recommendation (item 2 of #19).
11. Selective imports are preferred to importing a whole namespace. Original design.

### What an import makes visible

12. An imported file exposes only what it marks public; everything else is private to its file or namespace. How a declaration is marked public, and what `private` then means, are open (open question 1; #13). Direction.
13. Imports are not transitive: a name is reachable only by importing the namespace that defines it, not through a namespace that imports it. Original design.
14. A namespace name that collides with any other identifier is an error; importing the namespace under another name (point 9) resolves it. Original design.
15. `alias` renames anything (a function, a parameter, an enum value, a type) anywhere. Original design; it is not specific to modules, and the reference documents it nowhere else.
16. Imports may form cycles, in S0 too; a cycle is resolved by compile order across the files of a namespace. Original design; confirmed on #19 (direction).

### The standard library

17. Only `std.builtin` is reachable without an import (`std` is a keyword); every other `std.*` namespace is imported as any other is. Original design; recommendation that it stands (item 3 of #19).
18. This proposal decides no contents of the standard library. `HashMap`, `ArrayList`, `ArenaAllocator` and `PoolAllocator` were never discussed, and `dict[K,V]` and `list[T]` are built in; they leave the reference page until a proposal adds them. Recommendation (item 4 of #19).

### Interfaces

19. A compiled Cesium library comes with a `.csi` interface, the plain-text form of its public types and signatures, generated by the compiler. The library embeds a checksum of the interface, so that a mismatched interface is detected. A full copy of the interface inside the library, with a command that extracts it, is wanted later (#107). Direction.
20. Generating a C header for the functions a program exports to C is an opt-in compiler flag. Original design.

### S0

21. S0 has `extern` blocks (point 4) and one form of Cesium import: `import(ns, "path/to/file.csm")` (point 5) with a path to one source file, which defines `ns` and is the only file that does. Everything is linked statically; there are no compiled Cesium libraries and no interface files. Direction; these are the draft rules `s0.import` and `s0.import.public` of the specification.

### The reference page

Once the points above are accepted, the reference page "Modules and Imports" is corrected to match them. These are corrections, not design:

- `'c'`, `'m'` and `'kernel32'` become `"c"`, `"m"` and `"kernel32"` (point 8).
- `import('unistd')` becomes `import("c")`: `unistd.h` is a header, and `getpid` and `sleep` are in libc (item 6 of #19).
- `target_os` becomes `TARGET_OS`, as the built-in functions page has it (item 7).
- `math.vector.add(v1, v2)` calls a function the module does not define; the example uses `operator +` or `magnitude` instead (item 8).
- The `std.collections` and `std.memory` example goes (point 18).
- The file names are `.csm` and `.csi` (#14), as the page already has them; the issue's `.cesium` and `.m` were fixed before.

## Alternatives

- **Imports by file path only,** as C's `#include` and Zig's `@import("file.zig")`. Not taken: the original design keeps namespaces apart from file paths (point 1). Point 5 still gives a path where one is wanted, while the name stays the namespace's.
- **The assignment form, `io = import(std.io)`,** which matches `extern k32 = import("kernel32") { ... }`. Not taken by the direction on item 1 of #19 (point 9).
- **A list of the program's files,** in a manifest or on the command line, as stage 0 takes them today. Not taken by the direction of #9: one entry file, and the files its imports reach.

## Drawbacks

- An `extern` block names its library with an assignment (`extern libc = import("c")`), and a Cesium import with `as` (`import(std.io) as io`): two spellings for naming an import.
- With point 6, which files make a program depends on what the source roots hold: a stray file that defines the namespace changes the program, and every tool has to search the same roots.
- Namespaces across several files with cycles resolved by compile order presume that a namespace is compiled as a whole (#47, item 7), which ties this proposal to the compilation unit #47 has still to decide.

## Open questions

1. **Marking a declaration public** (point 12): a keyword such as `public` or `pub`, or `export`, which the reference already uses for C export and in import blocks (#47, item 6). And what `private` means at namespace level once everything not marked is private already (#13).
2. **The namespace a file defines:** the reference writes `namespace math.vector;` at the top of the file. Whether a file has at most one such line, and which namespace the entry file and a file without the line are in.
3. **Where an import is written and how it ends:** at the top level only, before the other declarations, or anywhere a declaration may be; `import(math.vector);` ends with `;` in the reference, and point 10 puts none after a block.
4. **Using an imported name:** the reference writes it qualified, `math.vector.magnitude(v)` and `windows.Sleep(ms)`. Stage 0 calls the functions of an `extern` block by their bare name, and `cesium1` is written that way; qualified names would change every call to C in it.
5. **`export` and the global forms:** `{ export Triangle }` in an import block and `export void = free(#void ptr);` in an `extern` block import a name "as global", and `#void = malloc(uword size) as malloc;` is a "global alias". Whether these stay, and how they agree with point 13 (#47, item 6).
6. **`as` on the other forms:** whether `import(ns, "path") as y` and `import(ns, name) as y` are written so.
7. **`as` and selective imports in S0:** whether S0 has either. Recommendation: neither until stage 1 needs one (P-0007, point 2).
8. **"Nothing scans a directory"** (`s0.program`, direction of #9) and points 5 and 6, where a directory import compiles the files of a directory and `import(ns)` searches the source roots. To confirm: the rule of #9 is about which files make a program (those its imports reach), and a search for the files of an import does not break it.
9. **Static or dynamic** for a library named by a bare name (point 7), as a compiler or build option, static by default: a recommendation on #107, not decided.

## Where it comes from

- #19 (moved from cesiumlang/cesiumlang.org#8, 2026-11-16): the items 1 to 9 of the reference page, and the rules of the original design.
- 2026-11-22, on #19: Cesium modules are imported in S0; an import names a namespace; `import(x) as y`; double quotes; cycles allowed; the import model (points 4 to 7, 19 and 21); visibility (point 12); items 2, 3, 4 and 6 to 9 left to the agent to propose.
- 2026-11-22, on #9, #47 (item 8) and #104: the entry file and the build script, which this proposal leaves to #104.
