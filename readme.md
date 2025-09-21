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
// hello.cs
void = main() {
  printf("Hello, Cesium!\n");
}
```

## Building

<!-- To build on Windows, you will need to install the [Windows SDK](https://developer.microsoft.com/en-us/windows/downloads/windows-sdk/).   -->
The current default builds use Zig and Clang, which can be installed via the script in [buildtools](https://github.com/cesiumlang/cesium-buildtools).
