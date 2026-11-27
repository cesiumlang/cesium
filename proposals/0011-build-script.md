+++
id = "P-0011"
type = "proposal"
status = "proposed"
+++

# Proposal P-0011: the build script, the entry point and the settings file

This proposal decides how the compiler learns what to build: the build script `build.csm`, the built-in that names the entry file, how a tool reads the script without building, and the settings file. It carries the design issue #104, which records the direction of emanspeaks of 2026-11-22 (on #9 and #47, item 8), and the source roots of the import model (#19). Points that the direction does not settle say that they are recommendations. emanspeaks accepts, changes or marks open each numbered point. Nothing in it is decided until he accepts it.

The forms of `import` are P-0010's; this proposal starts where they need an entry file and source roots.

## Motivation

Since the direction of 2026-11-22, a program is one entry file and the files its imports reach (#9). Something has to name that entry file, and every tool has to find it the same way: the compiler, the extension's language server (cesiumlang/vscode-cesium#7) and later `cesium lsp` (#68). Today `cesium0` takes the files of a unit on its command line, and `cesium1` is built from a file list in `build.zig`; nothing in the language says which files make a program. Stage 0's import (#105) waits on this and on P-0010.

## Design

### The build script

1. The build logic of a program is a Cesium script, `build.csm` by default. The default name is fixed in the compiler; the settings file can name another (point 12). Direction.
2. The compiler runs the script while building ("JIT"). Direction.
3. The script sets the entry file with a built-in function, something like `set_entrypoint()`, not with a line such as `entry = "path"`. Only build scripts the compiler reads directly can call it. Direction.
4. The entry function is `main`; naming another comes after S0. Direction.
5. The files of the program are the entry file and the files its imports reach, followed from the entry file; nothing scans a folder for them (P-0010 for the forms of `import`). Direction.
6. In S0 the script uses a small subset of Cesium; later stages grow it toward build logic in the manner of `build.zig`. Direction.
7. The script describes the build and does not carry it out: its built-ins record what to build, and the compiler builds once the script has returned. Recommendation: it is what makes point 8 possible, and it is how `build.zig` works (see Alternatives).

### Reading the script without building

8. A tool, such as a language server, evaluates the script without executing the build, so that conditions that choose the entry file or the source roots still give it the program's files. Direction.
9. By point 7, evaluating the script without building is running it and keeping what its built-ins recorded. A tool gives the script the same inputs the compiler would from the settings file (point 11), so that its conditions evaluate as they would in a build. Recommendation.
10. When the script cannot be evaluated (it does not compile, or it fails), the compiler reports the error and stops; a tool reports it as a diagnostic of the script and keeps the last result it had. Recommendation, as zls does (see Alternatives).

### The settings file

11. Compiler settings, the default arguments and their overrides, are in a JSONC file in the working directory or at the root of the repository; the build logic stays in the script. Direction. Every setting is a command-line option of the compiler, and the file gives its default value. Recommendation.
12. The settings file can name the build script, in place of `build.csm`. Direction.
13. The settings file is `cesium.jsonc`, looked for in the working directory, then at the root of the repository; the first one found is used, and none is needed. Recommendation: the name follows the `cesium.toml` of the first specification (#47, item 8).

### Source roots

14. Source roots, which `import(ns)` searches (P-0010, point 6), are set by the build script with a built-in, or by a compiler option, which the settings file can hold as a default argument (point 11). Direction: "the build script or the compiler options" (#19); recommendation: both, the second by way of point 11. Not in S0, which has only path imports.

### S0

15. In S0 a build script defines `void = build()`, which the compiler calls, and the body of `build` is calls of the built-ins with string literals as arguments. It declares nothing else and imports nothing. Recommendation: S0 has no top-level statements (`s0.unit`), the form follows `build.zig`'s `build` function, and a script of literal calls can be read by stage 0, which has no interpreter, without running anything.
16. The one built-in of S0 is `set_entrypoint("path/to/main.csm")`, with a path relative to the build script's directory. Recommendation; its name is the direction's "something like `set_entrypoint()`". The output file, the target and the optimization mode stay command-line options (point 11), as `cesium0` has them now.
17. The command line can name the entry file in place of a build script (`cesium0 -o out main.csm`), which compiles that file and the files its imports reach. Recommendation: the specification's examples and the tests compile one file each, with no build script.

```cesium
// build.csm
void = build() {
    set_entrypoint("src/main.csm");
}
```

## Alternatives

- **Zig's `build.zig`,** the model of the direction. `zig build` compiles `build.zig` and runs its `build` function, which builds a graph of steps; the steps run afterward. zls learns a project's modules by running `zig build --listen=-` and reading the configuration from the build system's protocol; of the steps it runs only those that generate source files, so that their paths can be resolved, and it passes the build options of its `zls.build.json` as `-D` arguments (zls, `src/bsp.zig`, `loadBuildConfiguration`, master branch, read 2026-11-27). Points 7, 9 and 10 follow it.
- **A declarative file only,** such as `cesium.toml` (#47, item 8) or `Cargo.toml`. Not taken: the direction puts the build logic in Cesium and only the settings in a JSONC file.
- **Top-level statements in the script,** as in a shell script. Not taken for S0 by point 15: S0 has no top-level statements.

## Drawbacks

- Two files can say how to build (the script and the settings file), and two can name the entry file (the script and the command line, point 17).
- Point 7 rules out a script that does work of its own while it runs, such as generating a file; that comes later as build steps.
- A JIT for the build script is needed by the compiler written in Cesium; until it has one, a script can be only what stage 0 reads without running it (point 15).

## Open questions

1. **The built-in's name** (point 16): `set_entrypoint`, or another; and whether it is reserved for build scripts as a name (`lex.predeclared.fixed`) or only callable there.
2. **The build function** (point 15): `void = build()` with no parameter, or with a parameter for the build, as `build.zig`'s `b: *std.Build`, once there are more built-ins.
3. **The settings file** (point 13): its name, whether "the root of the repository" is the directory holding `.git`, and whether a settings file in the working directory and one at the root are merged or the first one wins.
4. **The order of precedence** of the command line, the settings file and the build script when they disagree (an entry file on the command line and in the script, for instance).
5. **What the script may not do** in later stages (C calls, I/O, imports), so that a tool can always evaluate it (point 9).

## Where it comes from

- 2026-11-22, on #9 and #47 (item 8): the build script `build.csm`, run JIT, the entry point by a built-in, the entry function `main`, the settings in JSONC, the evaluation without building, and the S0 subset (points 1 to 6, 8, 11 and 12). Recorded on #104.
- 2026-11-22, on #19: source roots set by the build script or by compiler options (point 14).
