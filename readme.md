# The Cesium Programming Language

## Overview

Cesium is a compiled, statically-typed systems programming language designed for mathematical computing and linear algebra applications. It prioritizes performance, explicit memory management, and mathematical expressiveness while maintaining C ABI compatibility.

The language borrows design and syntactic elements from languages as varied as Python, Fortran, Rust, Go, Odin, and C++, but ultimately derives the bulk of its original design inspiration and goals from C, Zig, and MATLAB.  As such, an early placeholder name for the language was CZM for those three languages.

Ultimately, the name is a reference to the atomic element cesium (American Chemical Society spelling).  Since one of the primary objectives of the language is runtime performance, it seemed fitting to share a name with the primary element used for standard atomic clocks.  In fact, a second in timekeeping is officially defined in SI by assuming the unperturbed ground-state hyperfine transition frequency of the Cs-133 atom to be exactly 9,192,631,770 Hz.  It takes 34 bits to represent that number as an integer in binary.

Cesium provides a modern, performance-oriented programming language specifically designed for mathematical computing while maintaining systems programming capabilities. The language emphasizes:

- **Explicit control** over memory management and performance characteristics
- **Mathematical expressiveness** through context-aware operators and built-in mathematical types
- **Memory safety** through ownership tracking and compile-time borrow checking
- **Interoperability** with existing C libraries and systems
- **Predictable performance** through static dispatch and ahead-of-time compilation

The language strikes a balance between safety and performance, providing developers with the tools needed for both rapid mathematical prototyping and production systems development.

### Design Goals

- Math-first language with built-in mathematical operators and types
- Compile-time dispatch and optimization
- Memory safety through ownership tracking and explicit management
- Strong typing with minimal implicit conversions
- ASCII-only syntax for universal accessibility
- C interoperability for existing library integration

### Basic Program Structure

```cesium
// hello.csm
void = main() {
  printf("Hello, Cesium!\n");
}
```

## Status

The language design and the compiler's architecture are recorded as issues in this repository; they are the knowledge base. The decisions so far (label [`decision`](https://github.com/cesiumlang/cesium/issues?q=is%3Aissue+label%3Adecision)):

- The compiler is written in Cesium. A small stage 0 compiler in portable C compiles a tiny subset, S0, in which the first Cesium-written compiler (`cesium1`) is written ([ADR 0002](https://github.com/cesiumlang/cesium/issues/5), [stage 0 and S0](https://github.com/cesiumlang/cesium/issues/9), [stage 0 C dialect](https://github.com/cesiumlang/cesium/issues/10)).
- Every frontend lowers to a middle IR ([MIR](https://github.com/cesiumlang/cesium/issues/8)) with two backends behind it: QBE first, LLVM as the primary release backend ([ADR 0001](https://github.com/cesiumlang/cesium/issues/4), [ADR 0004](https://github.com/cesiumlang/cesium/issues/7)).
- `build.zig` is the build system and runs in CI; the toolchain (pinned Zig, LLVM and LLD libraries, QBE) comes prebuilt from [cesiumlang/devkits](https://github.com/cesiumlang/devkits).
- Source files end in `.csm`, module interface files in `.csi` ([#14](https://github.com/cesiumlang/cesium/issues/14)).

Open design questions carry the [`design`](https://github.com/cesiumlang/cesium/issues?q=is%3Aissue+is%3Aopen+label%3Adesign) label.

## Repository layout

[`bootstrap/`](bootstrap/README.md) is stage 0: the S0 compiler `cesium0`, the core MIR library `libcsmir` and a small runtime, in portable C, with their tests. `build.zig` builds and tests it with the Zig of the devkit:

```sh
python tools/fetch-devkit.py
.devkit/zig/zig build test
```

`src/`, `include/`, `cmake/` and `CMakeLists.txt` hold a C++ prototype from September 2025: a documentation generator with a CLI and a Qt GUI, built with CMake and the tools from [buildtools](https://github.com/cesiumlang/cesium-buildtools). It predates the decisions above and is not the compiler. What becomes of it is an open question ([#2](https://github.com/cesiumlang/cesium/issues/2)).
