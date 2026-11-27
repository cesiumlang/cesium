+++
id = "P-0002"
type = "proposal"
status = "proposed"
+++

# Proposal P-0002: compiler backend, LLVM packaging and toolchain

This proposal sets Cesium's release backend, how LLVM is obtained and linked, the toolchain that builds the compiler, the runtime model of Cesium programs and the target platforms. It carries the decision record cesiumlang/cesium#4 (dated 2026-11-15, moved to GitHub on 2026-11-16), which marked itself "Accepted (planning; implementation not started)" but was guidance written by an agent and was not accepted. emanspeaks accepts, changes or marks open each numbered point below; nothing in it is decided until he accepts it.

## Motivation

Cesium targets numerical methods, linear algebra and scientific computing, so release-build code quality (vectorization, FMA, aliasing, scheduling, register allocation) matters more than compile speed. The goals are a self-contained, portable compiler in the spirit of Zig's single-archive distribution; easy to build; small redistributable; C ABI compatibility for libraries; eventual self-hosting.

### Lessons from LFortran

LFortran (alpha, 9/10 toward beta as of early 2026) has a typed, semantics-rich IR (ASR) with about 200 passes, but most of them lower Fortran semantics (arrays, WHERE/FORALL, temporaries, intrinsics, optional arguments, OpenMP, coarrays; about 80 are GPU offload). Its CPU optimization passes are few (constant folding, dead code, `div_to_mul`, `sign_from_value`, inlining, allocatable promotion, loop fusion). Under `--fast` it runs LLVM's `default<O3>`, sets fast-math flags, marks dummy arguments `noalias` (F2018 15.5.2.13) and emits TBAA. Its non-LLVM x86 backend is about 1k lines against about 32k for `asr_to_llvm.cpp`.

- Study ASR's design (`asr.asdl`), the rationale of its pass order (`pass_manager.h`) and its lowering of array expressions (`array_op`, `where`, `array_struct_temporary`). LFortran is BSD-licensed.
- Diverge where LFortran lowers array operations to scalar loops and hopes LLVM vectorizes: Cesium's semantic IR lowers to explicit vector or tiled loops, using shape and alias knowledge directly.
- LLVM Flang's experience (MLIR/HLFIR above LLVM, still about 18% behind gfortran on Arm SPEC) confirms that array-level work above LLVM matters.

Other references for the optimizer: MLIR linalg/affine/vector dialects (design ideas), Halide and Exo (algorithm against schedule), Futhark (fusion), the BLIS microkernel decomposition (when to call kernels), Cranelift's e-graph mid-end and QBE's passes (scalar cleanup).

### Measurements (2026-11-15)

With the `x86_64-linux-musl` devkit from `zig-devkits` (Zig `0.18.0-dev.92`, LLVM 23.1.2):

- Devkit: 160 MB download, 1.2 GB unpacked; 207 `libLLVM*.a`, LLD libraries (`ELF`, `COFF`, `MachO`, `MinGW`, `Wasm`, `Common`), headers including `llvm-c`, plus `libz` and `libzstd`. No `llvm-config`, which is not needed: LLD resolves archives in any order.
- A prototype built with only the devkit's `zig cc` and `zig c++`, no CMake: parse IR through the C API, run `default<O3>`, emit assembly for x86-64 (AVX2 `ymm`, `vfmadd`) and AArch64 (`fmla`), then an object, then link with in-process LLD into a 1.7 KB static libc-free executable that runs. Separately, in-process `lld-link` produced a 2.5 KB PE32+ console executable against a `kernel32.lib` generated in-process from a `.def`.
- Compiler binary size, stripped: all targets 133 MB; X86 and AArch64 with LLD ELF only, 65 MB (17.8 MB xz).
- With stock devkits, LLD's ELF driver references "initialize all targets" (for LTO), so leaving targets out needed about 87 empty stub initializers. An LLVM built with only the needed targets removes this.
- Per-target static library sizes in the devkit: X86 21.9 MB, AArch64 21.4 MB, ARM 11.4 MB, PowerPC 7.4 MB, SPIRV 6.1 MB, WebAssembly 4.3 MB; for comparison AMDGPU 54.5 MB, RISCV 18.4 MB, Mips 6.3 MB, SystemZ 4.3 MB, LoongArch 3.2 MB.
- libc sizes (x86_64, stripped, -O2): static glibc hello 707 KB; static musl hello 7 KB, 30 KB with `printf("%.17g")` and `atof`, 49 KB adding `sin`, `exp` and `pow`; raw-syscall hello 1 KB; Windows raw `kernel32` hello 3 KB against 73 KB with the mingw CRT. Static linking pulls in only what is used; going libc-free buys independence and control more than size.
- The libc surface of LLVM, LLD and libc++ is about 250 functions (measured in #5; see P-0003).

### Sources the record cites

- Zig's self-hosted backends and its relation to LLVM: <https://kristoff.it/blog/zig-new-relationship-llvm/>, <https://ziglang.org/devlog/2025/>
- zig-bootstrap: <https://github.com/ziglang/zig-bootstrap>
- QBE: <https://c9x.me/compile/>, <https://c9x.me/compile/release/qbe-1.3.html>
- Cranelift against LLVM: <https://github.com/bytecodealliance/wasmtime/blob/main/cranelift/docs/compare-llvm.md>
- LFortran: <https://lfortran.org/>, <https://github.com/lfortran/lfortran>
- LLVM Flang performance (Arm, 2025): <https://llvm.org/devmtg/2025-10/slides/technical_talks/lupori.pdf>
- Odin install requirements: <https://odin-lang.org/docs/install>
- randyos-org repositories: `zig-build`, `zig-devkits`, `powerzig-bootstrap`, `powerzig`, `rstd` (`support.md`)

## Design

1. **LLVM is the primary (release) backend,** statically linked into the compiler together with LLD.
2. **A Cesium semantic IR is the core design asset.** It carries shapes, aliasing facts derived from the ownership and borrow rules (`:=`, `#`, `~`), fusion and explicit vector operations. Numerical optimization happens there; LLVM receives already-structured code plus facts it can exploit (`noalias`, fast-math policy, vector types). Its design is #8.
3. **A narrow LLVM boundary.** Cesium emits LLVM IR itself (textual first, bitcode later) and drives LLVM through a handful of LLVM C API calls: parse IR, run a pass pipeline string such as `default<O3>`, create a target machine, emit an object to memory.
4. **The C API, not the C++ API.** The C++ API adds custom passes inside LLVM's pipeline and analysis queries (alias analysis, SCEV, TargetTransformInfo), but breaks every major release (about twice a year) and would keep part of the compiler in C++ forever. Cesium speaks the C ABI, so the C API keeps the whole compiler portable to Cesium for self-hosting. Structural optimization happens in the semantic IR before LLVM, so passes inside LLVM are mostly unnecessary; target vector widths and features can come from a maintained table.
5. **The only C++ is a small shim** for LLD and for writing Windows import libraries, compiled with `zig c++ -c -fno-exceptions -fno-rtti -I<llvm>/include` and linked with `liblldELF`, `liblldCOFF`, `liblldMachO`, `liblldWasm` and `liblldCommon`. It registers the ELF, COFF, Mach-O and Wasm drivers and exports two C entry points:
   - `int cs_lld_link(int argc, const char **argv, int *can_run_again)` links with in-process LLD; `argv[0]` picks the flavor (`ld.lld`, `lld-link`, `ld64.lld`, `wasm-ld`), and `*can_run_again == 0` means LLD's global state is no longer usable in the process.
   - `int cs_write_import_lib(const char *def_text, size_t def_len, const char *out_path, unsigned short machine)` writes a Windows import library (such as `kernel32.lib`) from `.def` text through `parseCOFFModuleDefinition` and `writeImportLibrary` (MinGW mode), so Cesium needs no Windows SDK; `machine` is an `IMAGE_FILE_MACHINE_*` value (0x8664 x64, 0xAA64 arm64); it returns 0 on success.
   - The ELF and COFF paths and the import-library writer were tested against the devkit headers (LLVM 23.1.2); the Wasm driver lines were added after testing.
6. **Linking runs in a child process:** a `cesium ld.lld …` subcommand re-executes the compiler, as Zig does, so a linker failure that leaves LLD's global state unusable cannot take down a long-running compiler or language server.
7. **Prebuilt LLVM from a forked devkit pipeline.** When work starts, `randyos-org/zig-devkits` is forked into a Cesium pipeline that builds only LLVM core, the needed targets and LLD (no Clang, no LLVM tools), to keep the contributor download small. CMake runs only in that CI pipeline, never for contributors.
8. **The packaging pipeline:** pinned official Zig (through `setup-zig`), then host `llvm-tblgen`, then LLVM core, the targets of point 14 and LLD, static, no Clang, no tools; it publishes `include/`, the static libraries and a manifest (Zig version, LLVM version, target list, sha256). One package per host triple the compiler runs on; host packages are separate from the targets the compiler can emit, which are all built into every package. The same pipeline builds stage 0 and `libcsir`, runs the bootstrap chain, builds `libcsllvm` and publishes per-host bootstrap packages for contributors (#5, P-0003). No CMake and no system C or C++ compiler outside CI. Later options, not decided: size-optimized or LTO-friendly builds, an LLVM version independent of Zig's, local LLVM patches.
9. **Stock, pinned Zig is the C and C++ toolchain and build orchestrator in CI:** an official release (for example through `setup-zig`), pinned. The LLVM package is built with the same Zig version as everything linked with it (libc++ ABI consistency); the package is rebuilt whenever Zig is bumped, and the Zig version is recorded in its manifest.
10. **The compiler is written in Cesium.** The record's stage 0 and `libcsir` in C11, compiled by `zig cc` in CI only, with `build.zig` only in CI and Cesium's own build system for contributors once `cesium1` exists, is superseded by #5; P-0003 carries those decisions. (History in the record: first C, then briefly Zig, then back to C11 for the stability of the implementation core and native use of LLVM's C API.)
11. **Cesium programs are libc-free.** Cesium's own standard library talks to the OS directly. Third-party code linked through the C ABI may use libc; Cesium-native code does not. A Cesium program that links libc-dependent C libraries uses a hosted link mode (libc startup and thread creation; Cesium code still never calls libc). The compiler is such a program because of LLVM; how its libc is packaged in `libcsllvm` is #5 (P-0003).
12. **No toolchain dependency for users or, after `cesium1`, contributors.** Users get one self-contained compiler binary (LLVM and LLD inside) plus a few small bundled platform stub files: no Zig, no SDK, no system libc. Contributors download a per-host bootstrap package produced by CI (#5); Zig is needed only in the CI pipeline, and for stage 0 work before `cesium1` exists.
13. **Target platforms:** rstd's tier 1 and tier 2 platforms (`randyos-org/rstd` `support.md`), and WebAssembly as tier 2 for Cesium.

    | Platforms | LLVM target | LLD flavor |
    | --- | --- | --- |
    | x86_64: Windows, freestanding, UEFI, Linux (musl, gnu), macOS 11+ | `X86` | COFF, ELF, Mach-O |
    | aarch64: freestanding, UEFI, Linux, Windows, macOS 11+ | `AArch64` | ELF, COFF, Mach-O |
    | `arm-freestanding-eabi` (Raspberry Pi 3, 32-bit) | `ARM` | ELF |
    | `powerpc-freestanding-eabi` (iBook G3, PowerPC 750FX; soft- and hard-float) | `PowerPC` | ELF |
    | `spirv32-vulkan` (shaders; rstd tier 1) | `SPIRV` | none (LLD does not link SPIR-V) |
    | `wasm32-wasi`, `wasm32-freestanding` (Cesium tier 2) | `WebAssembly` | Wasm |

14. **The LLVM build:** `LLVM_TARGETS_TO_BUILD="X86;AArch64;ARM;PowerPC;SPIRV;WebAssembly"`, LLD with the ELF, COFF, Mach-O and Wasm drivers. UEFI images are PE/COFF through `lld-link /subsystem:efi_application`. rstd tier 3 would add RISCV, Mips, SystemZ and LoongArch (ppc64 and 32-bit x86 are already covered by `PowerPC` and `X86`).
15. **PowerPC 750** (32-bit big-endian, no AltiVec; LLD supports ppc32 ELF) gets its own CI smoke tests under qemu, as `powerzig` already does, because ppc32 codegen is less exercised in LLVM than x86 or ARM. The libc-free runtime needs a ppc32 syscall and OS layer.
16. **GPUs: SPIRV suffices; AMDGPU is not built.** Vulkan drivers compile SPIR-V to native ISA. AMDGPU (or NVPTX) would matter only for native ROCm/HIP (or CUDA) code; AMDGPU is also LLVM's largest target.
17. **SPIR-V through LLVM is tried first.** LLVM's SPIR-V backend is strongest for OpenCL-style kernels; its Vulkan shader path (logical addressing, `Shader` capability, structured control flow) is newer and less proven, and rstd's `spirv32-vulkan` uses Zig's own SPIR-V backend today. The plan is an early experiment emitting compute shaders through LLVM, validated with `spirv-val` and run on lavapipe; the fallback is a direct SPIR-V emitter from the semantic IR. Which of the two is open (Open question 4).
18. **WebAssembly:**
    - WASI preview 1 imports are the OS layer for `wasm32-wasi`; browsers need a thin generic host loader. WASI 0.2 is based on the component model and `wasm-ld` emits core modules, so components would need an extra wrapping step.
    - SIMD is fixed at 128 bits; the semantic IR's vector lowering must support that width.
    - `wasm32` is limited to 4 GB; `wasm64` (memory64) is tier 3 until engine support settles.
    - Threads need shared memory and atomics (browsers also need cross-origin isolation); single-threaded is the tier 2 baseline.
    - Testing can build on rstd's `wasm-test` rig.
    - FMA and NaN policy are open (Open question 5).
19. **The runtime model of Cesium programs:**
    - Linux: direct syscalls (stable ABI), fully static.
    - Windows: no CRT; `kernel32` and `ntdll` are called through import libraries the compiler generates at link time (LLVM's `writeImportLibrary`) from a small curated export list or mingw-w64 `.def` files.
    - macOS: no stable syscall ABI, so `libSystem` is linked dynamically (always present). LLD reads `.tbd` stubs; Cesium ships a trimmed `libSystem.tbd` listing only the symbols the runtime uses.
    - Freestanding, UEFI and RandyOS: the standard library's own platform layer (RandyOS is Linux-compatible, so the Linux layer applies); see #6 (P-0004).
20. **The runtime provides** `memcpy`, `memset`, `memmove` and `memcmp`; a math library; float formatting and parsing (Ryu or Dragonbox, Eisel-Lemire); an allocator, threads, files, time, arguments, TLS and startup. Owning libm gives bit-reproducible results across operating systems; which libm is open (Open question 7).
21. **Allocation across a C ABI boundary is handled by the ownership model.** An "unsafe mode" for crossing it is a later, bolted-on capability, not a current language-design problem.
22. **Possible later:** a fast, non-optimizing debug backend behind the same semantic IR (the Zig and Cranelift model), only if compile times become the main complaint.

## Alternatives

| Option | Why rejected |
| --- | --- |
| Fork Zig and build on ZIR/AIR | AIR is coupled to Zig's Sema/InternPool/Zcu internals; constant upstream churn (backends deleted or rewritten, std renames); AIR-to-MIR backends do no numerical optimization; a fork could not realistically be hooked back up later. |
| Emit Zig source | Some Cesium idioms do not map cleanly; Cesium would inherit Zig's ceiling and churn. |
| QBE as backend | No SIMD; roughly 70% of LLVM on integer-heavy code and weaker on floating-point and vectorizable loops; emits text assembly only (still needs an assembler, linker, unwind info, debug info); adding vectors means reworking its register allocator and instruction selection, in effect rewriting it. Remains a good reference for a small backend. |
| Own native backend as primary | A multi-year effort for one person (encoders, register allocation, ABIs, PE/ELF/Mach-O writers, unwind info, debug info, vectors). Zig, with a paid team, took about 5 years to make its x86_64 backend the debug default, and it is still slower than LLVM -O0. Deferred to an optional debug-only backend (point 22). |
| Emit C as the main path | A viable bootstrap, but an extra path to maintain once LLVM is used directly; C gives less precise control (UB, per-operation fast-math). Not needed for bootstrapping either (#5). |
| Cranelift | Not an optimizing compiler; roughly comparable to unoptimized LLVM. |
| Odin as a base | Not self-contained (needs MSVC and the Windows SDK on Windows, a system LLVM elsewhere); C++ over LLVM. Worth studying for array programming, `#soa` and matrices. |
| Build LLVM locally with CMake, or bundle Clang | Unneeded given prebuilt packages and libc-free programs. |
| Rust-, Swift- or Go-style toolchains | Heavy installs on Windows (Rust, Swift, clang, gcc); Go has garbage collection and weak numerical codegen. |
| The LLVM C++ API | See point 4. |

## Drawbacks

- The LLD and import-library shim is the code most likely to break on an LLVM upgrade: the signatures of `lldMain` and `writeImportLibrary` have changed before.
- Giving up the C++ API gives up custom passes inside LLVM's pipeline and LLVM's analysis queries (point 4).
- ppc32 codegen is less exercised in LLVM, and LLVM's Vulkan SPIR-V path is newer and less proven (points 15 and 17).
- The compiler binary is large: 133 MB stripped with all targets, 65 MB with X86, AArch64 and LLD ELF only (Measurements).
- The LLVM package must be rebuilt whenever Zig is bumped (point 9).

## Open questions

1. The design of the semantic IR: operations, vector types, shape and alias annotations, floating-point policy, lowering to LLVM IR. The record calls this the decision that is hard to reverse; the core level is sketched in #8.
2. Stage 0 and `libcsir` (S0 frontend, core SIR, LLVM IR) and the bootstrap chain: #5, #9.
3. Forking and trimming `zig-devkits` into the Cesium LLVM pipeline (targets and LLD drivers of point 14).
4. The SPIR-V experiment through LLVM's backend (validate and run), and the choice between LLVM and a direct emitter for shaders.
5. Wasm FMA and NaN policy relative to rstd's bit guarantees: standard `simd128` has no FMA and `relaxed-simd`'s `relaxed_madd` may or may not fuse, so either never fuse on Wasm or document Wasm as outside the bit-reproducibility guarantee; Wasm NaN bit patterns are nondeterministic, so check whether rstd's first-NaN rule relies on payloads.
6. Benchmark kernels (matvec, stencil, LU step, FMA reduction) to measure the quality of the semantic IR with LLVM, used as regression tests.
7. Which math library: a port of musl's MIT libm or Zig's `std.math`, or CORE-MATH for correctly rounded results.
8. The provenance and licensing of the trimmed `libSystem.tbd` stub, to check before redistributing it.

## What changed since the record

- The record itself states that its decision 6 (point 10 here) is superseded, and its decisions 7 and 8 (points 11 and 12) are refined, by #5 (dated 2026-11-15, revised 2026-11-16); P-0003 carries that record.
- #7 (2026-11-16) adds QBE as a second backend, used first for stage 0, `cesium1` and the start of the real compiler, and leaves LLVM as the primary release backend unchanged; P-0005 carries that record. This changes the order in which points 7 and 8 matter: the LLVM pipeline is no longer on the critical path to starting stage 0.
- The issue has no comments.
