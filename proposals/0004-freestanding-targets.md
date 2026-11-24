+++
id = "P-0004"
type = "proposal"
status = "proposed"
+++

# Proposal P-0004: freestanding targets

This proposal sets what the language, the standard library, the runtime and the compiler must provide so that Cesium can produce code that runs on bare metal: OS kernels, bootloaders, UEFI applications, firmware. It carries the decision record cesiumlang/cesium#6 (dated 2026-11-15, moved to GitHub on 2026-11-16), which marked itself "Accepted (planning; implementation not started)" but was guidance written by an agent and was not accepted. It relates to #4 (backend and targets, P-0002) and #5 (bootstrap and hosting the compiler, P-0003). emanspeaks accepts, changes or marks open each numbered point below; nothing in it is decided until he accepts it.

## Motivation

Cesium must be usable to write an OS that runs on bare metal. LLVM already generates bare-metal code well, so this is mainly a requirement on the language, the standard library and the runtime. rstd appears to be layered along similar lines already (point 5).

## Design

1. **The compiler runs only on hosted platforms but produces code for freestanding targets.** Producing bare-metal code is primarily a language, standard library and runtime requirement.
2. **Host and target are independent.** The target triple is a first-class compiler input; nothing about the host leaks into the output.
3. **A freestanding target implies** no implicit startup code, no OS layer, no heap unless the program supplies one, and no threads or TLS unless the program supplies them.
4. **Using an OS-dependent API from freestanding code is a compile error,** not a link error.
5. **The standard library is layered** in the style of Rust's `core`, `alloc` and `std`:
   - `core`: no OS, no allocation: math, linear algebra kernels, formatting into buffers the caller provides, atomics.
   - `alloc`: the allocator interface; the program provides the allocator.
   - `os`: the platform layers (Linux and RandyOS, Windows, macOS, WASI, UEFI, and more).
6. **On freestanding targets the panic handler is provided by the user,** and there is no unwinding.
7. **Cesium ships the compiler-runtime builtins for every target,** as Zig ships `compiler_rt`, because LLVM emits calls to helper functions when a target lacks an instruction:
   - 64-bit division and similar on 32-bit ARM and PowerPC (`__udivdi3`, `__aeabi_*`);
   - all floating point on the soft-float PowerPC 750 build;
   - atomics libcalls on older cores;
   - `memcpy`, `memset`, `memmove`, `memcmp`.

   How they are provided is open (Open question 1).
8. **Controls for kernel code.** The language and code generation provide the following, and the middle IR carries them through to LLVM; most map onto LLVM IR attributes and target features:

   | Need | LLVM mechanism |
   | --- | --- |
   | Inline assembly, naked functions | inline asm, `naked` |
   | Interrupt and exception handlers | calling conventions such as `x86_intrcc`, ARM IRQ |
   | Kernel code that must not touch FPU or SIMD registers | per-function `target-features` (for example `-sse,+soft-float`; general registers only on AArch64) |
   | No red zone in x86-64 kernel code | `noredzone` |
   | Kernel code models; static, PIC or PIE relocation models | target machine options |
   | Explicit sections and symbol placement | `section`, `align` |
   | MMIO and device registers | `volatile` loads and stores; explicit layout and packing |
   | Atomics with explicit memory ordering | atomic instructions |
   | Per-function CPU features | `target-features` |
   | Stack probes off or controlled (Windows and UEFI insert `__chkstk`) | `probe-stack` |

9. **ELF kernels with linker scripts** (LLD supports them).
10. **PE/COFF UEFI applications** through `lld-link /subsystem:efi_application`, already covered by the LLD shim (P-0002, point 5).
11. **Raw flat binaries** for bootloaders and firmware. LLVM's objcopy library has no C API, so either the shim gains a function or Cesium gets a small extractor from ELF to flat binary, written in Cesium (the record calls it easy); which is open (Open question 2).
12. **DWARF debug info,** for debugging under QEMU with gdb.
13. **Freestanding CI follows rstd's QEMU rigs:** x86_64, aarch64, arm (soft-float and hard-float), powerpc-750 (soft-float and hard-float), x86_64-uefi and aarch64-uefi.
14. **The `core` layer and the compiler-runtime builtins come first.** They must exist before any freestanding target works, and hosted targets build on them too.
15. **The middle IR has per-function attributes from the start** (calling convention, target features, red zone, stack probes, sections), not as an afterthought.
16. **Stage 0 needs no freestanding support,** but the compiler written in Cesium does not assume a hosted target anywhere in its code generation path.

## Alternatives

The record gives none.

## Drawbacks

The record names none. Its consequences (points 14 to 16) set work that has to come before any freestanding target works.

## Open questions

1. How the compiler-runtime builtins are provided: written in Cesium, a port of Zig's `compiler_rt` (MIT), or LLVM's compiler-rt builtins (Apache-2.0 with LLVM exception).
2. How raw flat binaries are produced: a shim function into LLVM's objcopy library, or a small extractor from ELF to flat binary written in Cesium.

## What changed since the record

- 2026-11-16, recorded in #7 (P-0005): the QBE backend does not provide the freestanding attributes (interrupt calling conventions, naked functions, sections, no red zone, stack probes), so building a kernel requires the LLVM backend.
- The issue has no comments.
