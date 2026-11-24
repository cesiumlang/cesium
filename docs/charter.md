+++
id = "P-0001"
type = "proposal"
status = "proposed"
+++

# Charter (P-0001)

What Cesium is for, what it is not for, and the principles its design choices are weighed against. It is short and meant to change rarely; a change to it is a proposal like any other (%[P-0000]). This proposal makes it the charter when emanspeaks accepts it.

## Goals

1. **A compiled language for mathematical computing that is also a systems language.** Mathematical code (linear algebra, numerical methods) should be quick to write, and the same language must reach the operating system and the machine as C does. Languages such as Fortran, MATLAB and Julia make systems code hard to write; Cesium does not wall the developer off.
2. **Performance.** Code compiles to efficient machine code. Speed is the first measure of a design choice that affects what the program does at run time.
3. **Control over memory, with more safety than C.** Memory is managed explicitly, without a garbage collector, and ownership is tracked by the compiler, so that the common mistakes of C are caught without the weight of a heavy type system.
4. **Within reach of developers new to systems programming.** People who know a math language, and find C unsafe or Rust daunting, should be able to write correct systems code, alone or in a team.
5. **Interoperability with C.** Cesium calls C libraries and is called by C, through the C ABI.
6. **Stability.** A program that is valid today stays valid with the same meaning: the language is designed with its fundamentals settled early, not changed in a break later.

## Non-goals

- **Garbage collection,** or any runtime that manages memory behind the program.
- **Type inference** of declarations: a variable's type is written. (Built-ins such as `alloc` and literals may take their type from their context.)
- **Dispatch at run time:** no virtual functions; dispatch is resolved at compile time.
- **A single paradigm:** procedural and object-oriented styles are both supported; neither is forced.
- **Hidden state in names:** no transitive imports, no namespaces imposed by file paths, no reserved method names a type must use.
- **Guarding the developer against every mistake at run time:** checks that cost performance in a release build are not imposed; the developer owns the program's behavior (for example, integer overflow wraps).

## Principles

1. **Explicit over implicit.** What a line does is visible in it: types are written, conversions that lose information are written, special mechanisms look special (`$name$` operators, address-context operators). Readability wins over typing less.
2. **No hidden cost.** The language does not insert conversions, allocations or checks the program did not ask for.
3. **Resolved at compile time.** Dispatch, overloads, operators and generics resolve when the program is compiled; operators are notation for functions.
4. **Fundamental operations belong to the language.** Memory (`alloc`, `free`), I/O and the standard mathematical functions are built in, not taken from a library. The memory built-ins are never shadowed; the mathematical ones are predeclared names a program may shadow (#50).
5. **ASCII syntax.** Every keyword, operator and punctuation mark is ASCII.
6. **The developer is responsible.** The language makes the safe path easy and the unsafe path visible, and otherwise trusts the developer.
7. **Optional, not imposed.** A feature a developer does not want need not be used; the core stays small enough to learn.

## When principles conflict

In this order, unless a proposal argues otherwise for one case:

1. Run-time performance.
2. Correct and explicit meaning (readability, no hidden cost).
3. Interoperability with C.
4. Ergonomics for the developer.
5. The simplicity of the compiler, and the size of the generated code.

So a feature may make the compiler more complex if it makes programs better to write and costs nothing at run time; it may not cost run-time performance to save the compiler work.

## About this proposal

### Motivation

Proposal P-0000 sets up a charter as the first layer of the documentation: the goals and principles that every later proposal is weighed against. Until now they were spread over the overview page of cesiumlang.org ("Design Goals") and the design conversations of September 2025.

### Sources

- The "Design Goals" of the overview page of the reference: math-first, compile-time dispatch, memory safety through ownership tracking, strong typing with few implicit conversions, ASCII-only syntax, C interoperability.
- emanspeaks's statements in the design conversation of 2025-09-08 to 11, on performance, the founding problem (math languages that wall off systems code), the audience, explicitness, developer responsibility, compile-time resolution, C interoperability, and the priorities above.
- The decisions recorded since in cesiumlang/cesium (#9, #11, #14) and emanspeaks's directions of 2026-11-22 to 24, which these goals are consistent with.

### Alternatives

- **No charter:** each proposal argues from scratch. Rejected by P-0000.
- **A longer charter** that also states the memory model or the type system. Those are the subject of their own proposals; the charter only gives what they are judged by.

### Drawbacks

- A priority order is a simplification: some decisions will weigh several principles at once. The order is a default, not a rule.

### Questions for emanspeaks

Points the sources do not settle in emanspeaks's own words, on which this draft took the overview page or a recommendation:

1. **ASCII-only syntax** (principle 5) is on the overview page, and no conversation states it in emanspeaks's words. Keep it?
2. **I/O as part of the language** (principle 4): the 2025 summaries say "first-class I/O"; only `alloc`, `free`, `file` and the math functions are stated by emanspeaks. Keep I/O there?
3. **The priority order:** performance first and the compiler's simplicity last are emanspeaks's words; the place of C interoperability (third) and of ergonomics (fourth) is this draft's reading. Right order?
4. **A REPL** (an interactive mode, "akin to Jupyter notebooks") was a wish in 2025, as long as compiled performance is not compromised. A goal, or left out?
