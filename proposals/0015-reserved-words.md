+++
id = "P-0015"
type = "proposal"
status = "proposed"
+++

# Proposal P-0015: reserved words and predeclared names

This proposal decides which words are keywords, which names are predeclared and may be shadowed, and which are predeclared and never shadowed. It carries the design issue #50, with the answers emanspeaks gave there on 2026-11-22. The issue leaves the exact lists to the proposal; the points that make them say they are recommendations. emanspeaks accepts, changes or marks open each numbered point. Nothing in it is decided until he accepts it.

Which built-in functions exist is #20's; whether a declaration may reuse a name the program itself declared is #75's.

## Motivation

The reference page "Lexical Structure" reserves about 80 words, among them names that are natural names of variables, fields and functions (`list`, `path`, `type`, `error`, `file`), and no page says which built-in names are reserved and which may be shadowed (#20). A keyword added later breaks every program that uses the word as a name, so the set is chosen at the start. S0 is frozen once stage 1 works (#9), and stage 0 already rejects every keyword of full Cesium as a name.

## Design

Each point names where it comes from: a direction of emanspeaks on #50 (2026-11-22), a decision of #9, a rule of the original design, or a recommendation.

### Keywords

1. The built-in types are keywords, not names in an always-imported namespace: `file`, `str`, `list`, `dict`, `slice`, `path`, `uword` and the others. Direction; the draft rule `lex.keywords.types`.
2. The numeric types are among them: the integer types, `u` or `i` followed by a bit width (`u8`, `i32`, `u34`), and the floating-point types `f16`, `f32` and `f64`. Direction, as this proposal reads "the other built-in types"; the specification still lists the sized type names as open (open question 8). The reference page "Type System" has the arbitrary widths.
3. `import` is a keyword. Recommendation: P-0010 and S0 (`s0.import`, `s0.extern`) use it as one, and the reference's list leaves it out (#20).
4. The words of the list that begin or qualify a declaration stay keywords while their constructs are in the language: `enum` and `union`, as `struct` is; the qualifiers `static`, `secret`, `register`, `context` and `property`; and `super`, which begins a statement (`super Point(x, y);`). Recommendation: the parser has to know them before the declaration's type and name (`static Point = origin()`, `enum Status { ... }`), which is the criterion of #50's suggestion ("only words the grammar needs"); a construct dropped later, as `register` may be (#58), frees its word.

### Predeclared names

5. The predeclared names are those of `std.builtin`, which every file sees without an import (P-0010, point 17). Original design.
6. The mathematical built-ins (`min`, `max`, `abs`, `mean`, `norm`, `round` and the like) are predeclared names, which a program may shadow. Direction; the draft rule `lex.predeclared.shadowable`.
7. The other predeclared functions and constants, whichever #20 keeps (`printf`, `debugf`, `assert`, `floor`, `ceil`, `trunc`, `pi`, `stdin`, `stdout`, `stderr`), are predeclared names a program may shadow, not keywords. Recommendation: the direction names only `std`, `alloc` and `free` as never shadowed, these are values and functions that no rule of the grammar needs, and the reference lists them as built-ins, not as keywords.
8. A shadowed predeclared name stays reachable as `std.builtin.name`, such as `std.builtin.min`. Original design (the design chat of September 2025, as #20 records it).
9. A predeclared name may be shadowed wherever a declaration may be written, at the top level or in a block; #75 decides only about names the program declares. Recommendation: the direction lets a program shadow them and sets no limit on where.

### Never shadowed

10. `std` and the memory built-ins `alloc` and `free` are never shadowed, even where they are not keywords: a declaration of one of these names is an error. Direction; the draft rule `lex.predeclared.fixed`.
11. `realloc` is never shadowed either. Recommendation: the reference lists it with `alloc` and `free` as the memory built-ins, and S0 leaves out `free` and `realloc` together (`s0.excluded`).

### S0

12. S0 reserves every keyword of Cesium, also those it does not use: an S0 program is a Cesium program (`s0.subset`), so it cannot name a variable `list`. Decided in #9, rule 1; stage 0 already rejects such names.

### The reference page

Once the points above are accepted, the page "Lexical Structure" is corrected to match them. These are corrections, not design:

- The keyword list gains `import` (point 3) and the numeric types (point 2), and loses whatever the open questions below free.
- "Built-in Functions and Constants" becomes the list of predeclared names, with `alloc`, `free` and `realloc` marked as never shadowed (points 7, 10 and 11).

## Alternatives

- **The built-in types as names in an always-imported namespace,** shadowable and reachable as `std.builtin.list` (#50's suggestion). Not taken: the types stay keywords (point 1).

## Drawbacks

- Common names stay unavailable: `list`, `path`, `file`, `str`, `slice`, and by point 2 the loop counters `i1`, `i2` and `u1`.
- A program cannot define its own `str` or `file` in place of the built-in one, as a freestanding program might want to (#6, P-0004).
- Two kinds of reserved name, the keyword and the predeclared name that is never shadowed, have the same effect on a declaration.

## Open questions

1. **`type`:** reserved, or freed: no construct of the reference uses it, and `typeof` is a keyword of its own. Recommendation: freed, so that a field or a parameter may be called `type`.
2. **`error`:** a keyword, as the declarations `error FileNotFound { ... }` need, or a word recognized only at the start of a declaration, so that `error` stays a name elsewhere; error types themselves are open (#17, #40).
3. **`this`:** a keyword, or a name predeclared inside methods.
4. **Other memory built-ins:** whether `copy`, `zero`, `compare` or `move` of the page "Built-in Functions" exist (#20), and if so whether they are never shadowed.
5. **`true` and `false`** (`lex.predeclared.truth`): shadowable, or never shadowed.
6. **Fields and methods named `free` or `alloc`:** whether a field, reached only as `x.free`, may take a name that is never shadowed.
7. **Integer widths:** which widths `uN` and `iN` may have, and so which names point 2 reserves.
8. **The reading of point 2:** whether "the other built-in types" of the answer covers the sized type names (`u8`, `i32`, `f64`), so that they are keywords, or they are predeclared names; the specification leaves it open (the open point after `lex.keywords`), and P-0017, point 8, holds either way.

## Where it comes from

- #50 (filed 2026-11-16 from the design audit): the list of common words, and the suggestion.
- 2026-11-22, on #50: the types stay keywords, the mathematical functions are shadowable, `std`, `alloc` and `free` are never shadowed (points 1, 2, 6 and 10); the exact lists and the interaction with #20 and #75 left to the proposal.
- #20: the design chat of September 2025, with `alloc`, `free`, `file` and `std` not shadowable, and the mathematical functions shadowable and reachable as `std.builtin.*` (point 8).
