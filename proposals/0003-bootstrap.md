+++
id = "P-0003"
type = "proposal"
status = "proposed"
+++

# Proposal P-0003: bootstrap strategy and hosting the compiler

This proposal sets what language the compiler is written in, how it bootstraps, what the CI pipeline produces, what contributors need, how the compiler (which contains LLVM) gets a libc, and how Cesium is ported to new hosts. It carries the decision record cesiumlang/cesium#5 (dated 2026-11-15, revised 2026-11-16, moved to GitHub on 2026-11-16), which marked itself "Accepted (planning; implementation not started)" but was guidance written by an agent and was not accepted. The record supersedes decision 6 of #4 and refines its decisions 7 and 8 (points 10 to 12 of P-0002). emanspeaks accepts, changes or marks open each numbered point below; nothing in it is decided until he accepts it.

## Motivation

The compiler is meant to be a Cesium program like any other, yet something has to build the first one, and contributors should need no toolchain to work on it. The compiler also contains LLVM and LLD, which are C++ and need libc++ and a libc, while Cesium programs are libc-free by default.

### Why portable C for stage 0 and `libcsmir`, not Zig

- **Stability of the implementation core.** Zig is pre-1.0; its releases bring breaking language and standard-library changes and occasionally new compiler bugs. Stage 0 and `libcsmir` are the root of the bootstrap chain and should not need rework on every Zig upgrade. ISO C is frozen.
- **Native fit with LLVM.** LLVM's C API is C; calling it from C needs no binding layer (no translate-C step, no hand-written `extern` declarations).
- **Portability of the root.** Restricting stage 0 to the subset every mainstream compiler implements (Clang or `zig cc`, GCC, MSVC, with TinyCC or cproc as a canary) lets Cesium move away from `zig cc` as the C compiler later without touching stage 0.
- **Safety.** A banned-functions list modeled on git's `banned.h` and the Linux kernel's deprecated interfaces, enforced at compile time, reduces the risk of memory-safety bugs in the root of the chain.
- **Zig's churn stays in `build.zig`,** which affects only orchestration.

The C dialect's limits apply only to stage 0 and `libcsmir`; they do not affect Cesium's language design or the real compiler. Details, flags and CI enforcement are in #10.

### The compiler and libc

When libc code runs inside a program whose startup is Cesium's:

- **Startup:** LLVM needs libc initialized first (TLS for `errno` and allocator state; C++ static constructors, of which LLVM has many). A bare Cesium `_start` skips this.
- **Threads:** on Linux, calling libc from a thread created by raw `clone` instead of `pthread_create` is undefined; LLVM may start its own pthreads.
- **Ownership:** strings and buffers LLVM allocates must be freed with LLVM's `Dispose` functions, never with Cesium's allocator.
- **macOS** mostly avoids the startup and thread problems because the OS always loads and initializes `libSystem`.

### Measured libc surface of LLVM, LLD and libc++ (2026-11-15)

Linking the prototype (LLVM with X86 and AArch64, LLD ELF, COFF and Mach-O, the shim, Zig's libc++, libc++abi and libunwind) with no libc left about 250 undefined functions:

- memory: `malloc`, `free`, `realloc`, `aligned_alloc`, `mmap`, `munmap`, `mprotect`, `madvise`
- files and directories: `open`, `read`, `write`, `stat`, `fstat`, `lseek`, `pread`, `opendir`, `readdir`, `getcwd`, `realpath`, `rename`, `unlink`, `mkdir`, `readlink`, `fopen`, `fread`, `fwrite`, `fseeko`
- threads: about 30 `pthread_*`, `sched_getaffinity`, `sysconf`
- processes and signals: `fork`, `execve`, `posix_spawn*`, `wait4`, `sigaction`, `sigprocmask`, `sigaltstack`, `raise`, `kill`
- formatting and parsing: `snprintf`, `vfprintf`, `vasprintf`, `sscanf`, the `strtod` and `strtol` families, string functions
- locale and wide characters (mostly for libc++): `newlocale`, `uselocale`, `freelocale`, `isw*_l`, `mbrtowc`, `wcrtomb`, `strftime_l`, `strcoll_l`
- math: `pow`, `atan2`, `log1p`, `erf`, `cosh`, and more
- other: `dlopen`, `dlsym`, `dladdr`, `dl_iterate_phdr`, `setjmp`, `longjmp`, `clock_gettime`, `uname`, `getrlimit`, `getpwuid_r`

## Design

1. **The Cesium compiler is written in Cesium.** It is a Cesium program like any other.
2. **Stage 0 is written in portable C, compiled by `zig cc` in CI, and lives only in CI.** Portable C means the C99/C11 intersection every mainstream compiler implements, no extensions, the ISO C library only, and a banned-functions list (#10).
3. **Stage 0 compiles S0 to core MIR.** Stage 0 compiles a tiny Cesium subset (S0) to the core level of the middle IR (MIR), then lowers MIR to LLVM IR through `libcsmir` (portable C: MIR parser, verifier, printer, lowering from MIR to LLVM IR) and `libcsllvm` (the LLVM C API plus the C++ LLD shim). `cesium1`, written in S0, is only a frontend that emits core MIR text and calls `libcsmir`'s coarse C API, so it is self-contained from the start. The real compiler later reimplements MIR and its lowering in Cesium.
4. **A layered from-source chain, run in CI.** Stage 0 compiles stage 1 (written in S0); further layers are frozen snapshots, made only when the compiler needs new language features. The chain ends with the full compiler rebuilding itself, checked for identical output.
5. **Chain maintenance.** Every layer stays buildable by the one below. A layer is frozen only when the compiler genuinely needs new features; S0 stays deliberately small, and the compiler avoids fancy features until it is self-hosted. Frozen layers keep their own `libcsmir`, so the real compiler's MIR can evolve freely.
6. **The chain is combined with previous-release-style distribution.** The layered chain runs only in CI and gives auditability and reproducibility; contributors get packages, which keep their setup trivial.
7. **The CI pipeline produces per-host bootstrap packages.** Contributors download a package and build with Cesium's own build system; they need no Zig, no C or C++ toolchain, no CMake. A package holds the `cesium` compiler and `libcsllvm` for that host; if pre-linking does not work out, it holds instead the LLVM and LLD static libraries, the shim archive, the libc++, libc++abi and libunwind archives and the host's libc pieces (musl `libc.a`, the mingw CRT and import libraries, `libSystem.tbd`), all produced by Zig in CI.
8. **`build.zig` runs only in CI pipelines.** It orchestrates the LLVM build, stage 0, `libcsmir`, `libcsllvm` and the chain. Zig's release churn is accepted there because it affects only build orchestration, not the implementation. Once the pipeline has produced `cesium1`, Cesium builds itself with its own build system.
9. **The CI pipeline** (forked `zig-devkits`, `build.zig`, pinned official Zig), for each release:
   1. Build host `llvm-tblgen`, then LLVM core, the targets (`X86;AArch64;ARM;PowerPC;SPIRV;WebAssembly`) and LLD (ELF, COFF, Mach-O, Wasm), static, no Clang, no tools.
   2. Build the LLD shim with `zig c++`; build `libcsllvm` per host.
   3. Build stage 0 and `libcsmir` (portable C, with `zig cc`, plus the portability and banned-function checks of #10) and run the layer chain to the full compiler; verify that the compiler rebuilds itself identically.
   4. Cross-compile the compiler for every host triple (the same machinery as `build-llvm-cross.sh`); smoke-test natively or under qemu.
   5. Publish one bootstrap package per host, with a manifest (Zig version, LLVM version, target list, chain commits, sha256).
10. **`libcsllvm`:** one archive per host that pre-links LLVM, LLD, the shim, libc++ and (where applicable) a libc, exporting only the LLVM C API functions and shim entry points Cesium calls, including a coarse `cs_compile_ir` (IR text in, object out), with everything else hidden and unused code garbage-collected. This shrinks the contributor download and keeps the libc an internal detail. Its exact form is open (Open question 2).
11. **The Cesium code in the compiler never calls libc;** the libc that LLVM needs is an internal detail of `libcsllvm`. Per host:

    | Host | libc inside `libcsllvm` |
    | --- | --- |
    | Linux (musl and gnu hosts) | static musl |
    | RandyOS | the static `linux-musl` build, unchanged (RandyOS will be Linux-compatible) |
    | Windows | mingw CRT (from Zig) |
    | macOS | system `libSystem` (dynamic; always present) |
    | FreeBSD, NetBSD | system libc (always present in the base system); musl over Cesium is possible where the syscall ABI is stable |
    | OpenBSD | system libc required: as far as known, OpenBSD permits system calls only from its own libc |
    | New OS with no libc or foreign syscalls | musl over Cesium's platform layer |

12. **Hosted link mode, in general.** A Cesium program that links any libc-dependent C library (LLVM, OpenBLAS, Qt, ...) uses libc's startup and creates threads through it, while Cesium code still never calls libc directly.
13. **"musl over Cesium" where it applies, preferred.** musl's internal syscall layer is replaced by calls into Cesium's platform layer; Cesium's threads and startup then are the libc's, so the compiler becomes an ordinary libc-free Cesium program that happens to contain LLVM, and the problems of hosted mode go away.
14. **LLVM's allocations are freed with LLVM's `Dispose` functions,** never with Cesium's allocator; the binding layer can enforce this.
15. **The libc surface is regenerated in CI on every LLVM bump,** to show new libc requirements. For early bring-up on a new platform, LLVM built with `LLVM_ENABLE_THREADS=OFF` shrinks the pthread part, and much of the rest can be stubbed (C locale only, a failing `dlopen`, no `fork` if the compiler avoids spawning processes).
16. **No wasm in the bootstrap.** Porting to a new host is done by cross-compilation from the CI pipeline. A `wasm32-wasi` build of the compiler remains useful for a browser or online playground (Wasm is Cesium tier 2), not for bootstrapping.
17. **Bare metal is a target, not a host.** A freestanding compiler host only makes sense with an OS underneath (LLVM needs files, memory mapping, threads); see #6 (P-0004).
18. **Porting to a new host:**
    1. Add the LLVM target if the architecture is new (rebuild the LLVM package).
    2. Write Cesium's standard library platform layer for the new OS and, if it lacks a usable libc, the musl-over-Cesium syscall layer.
    3. In CI, cross-compile LLVM, the runtime archives, `libcsllvm` and the compiler with `zig cc` for the new host (Zig has no OS tags for an OS like RandyOS, so use `-target <arch>-freestanding` with Cesium's own libc headers and archives, or `<arch>-linux-musl` for Linux-compatible systems), then publish its package.
19. **Phases:**
    - Before `cesium1`: compiler work happens in stage 0 (portable C) and the first compiler written in Cesium; developers need the pinned Zig as C compiler and build system.
    - After the pipeline publishes `cesium1`: contributors use only the bootstrap package and Cesium's build system.
    - Ongoing: the from-source chain runs in CI for every release as a check of auditability and reproducibility.

## Alternatives

| Model | Needs | Cost |
| --- | --- | --- |
| Previous-release binary (Rust, Go) | A published Cesium binary per host | Trust in binaries; new hosts need cross-compiling |
| Wasm seed and `wasm2c` (Zig) | A C backend, `wasm2c`, a WASI shim, a checked-in blob | A permanent C backend; managing the blob |
| Layered chain from a maintained root (GNU Mes or live-bootstrap style) | A root compiler in another language, frozen layers, the LLVM package | The chain grows; slower from-scratch builds |

The record chooses the third, combined with the distribution of the first (point 6).

- **Zig's wasm seed.** Zig checks in a 3.2 MB `zig1.wasm` (an older Zig compiler built without LLVM, with only its C backend, targeting `wasm32-wasi`), plus `wasm2c.c` (about 2,300 lines), `wasi.c` (about 1,100 lines) and `zig.h` (about 6,800 lines, the support header for C-backend output). Its bootstrap: `cc` builds `wasm2c`, which translates `zig1.wasm` to C; that builds `zig1`, which compiles the current compiler through its C backend to `zig2.c`; `cc` builds `zig2` linked with LLVM, and `zig2` builds an optimized `stage3`. The `wasm2c` step exists because Zig must bootstrap on any host with only a C compiler (no LLVM, no wasm runtime, no prebuilt host binary). Cesium does not share that constraint: the pipeline already has the pinned Zig and the LLVM package. The wasm seed would add a permanent C backend, `wasm2c`, a WASI shim and a checked-in trusted blob, for no benefit.
- **Zig for stage 0 and `libcsmir`.** Rejected for the reasons under Motivation; Zig stays in `build.zig` only.
- **A helper process holding LLVM (`cesium-backend`).** It would also isolate libc, but is overkill.
- **`libcsllvm` as a shared library with an export list.** The fallback if a partial link does not work, at the cost of single-binary distribution (Open question 2).

## Drawbacks

- The layered chain grows, and builds from scratch are slower.
- Every layer must stay buildable by the one below, which constrains the compiler's use of language features until it is self-hosted.
- Zig's release churn is accepted in `build.zig`.
- Before `cesium1`, developers need the pinned Zig.
- Hosted link mode brings the startup, thread and ownership problems listed under Motivation wherever musl over Cesium does not apply.

## Open questions

1. Stage 0 in portable C: the definition of S0, the design of core MIR and the C dialect (#9, #8, #10).
2. The `libcsllvm` experiment: the cleanest form is a partial link (`ld.lld -r` with garbage collection), but whether `ld.lld` supports `-r` together with `--gc-sections` well enough is unverified; the fallback is a shared library with an export list. To be tried when the pipeline is built.
3. The design of hosted link mode in the Cesium runtime: startup order, thread creation, ownership at the LLVM boundary.
4. musl over Cesium: map musl's internal syscall layer onto Cesium's platform API; prototype on Linux first to compare against stock musl.
5. A reproducibility check for the self-rebuild and the cross-built packages.
6. Confirm RandyOS's Linux syscall coverage against the measured libc surface (memory mapping, files, `clone` and `futex`, signals, time, TLS setup).

## What changed since the record

- 2026-11-16, recorded in the body: stage 0 is built around core MIR (#9, #8), and stage 0 and `libcsmir` are written in a portable C subset (#10) rather than Zig. The text above already includes this revision.
- 2026-11-16, recorded in #7 (P-0005): stage 0 and `cesium1` use QBE, whose assembly the pinned `zig cc` assembles and links in CI, so no LLVM build, devkit fork or `libcsllvm` is on the critical path to starting stage 0; the LLVM backend and `libcsllvm` are added later. This changes the order of points 3, 9 and 10: #7 states that it changes the order in which backends appear in the chain.
- The issue has no comments.
