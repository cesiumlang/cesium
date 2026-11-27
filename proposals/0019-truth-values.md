+++
id = "P-0019"
type = "proposal"
status = "proposed"
+++

# Proposal P-0019: truth values

This proposal decides how Cesium writes truth without a boolean type: `true` and `false` as `u8` constants, what a condition may be, and the values of comparisons and of the logical operators. It carries the design issue #38, with the answers emanspeaks gave there on 2026-11-22. Where the issue leaves a point to the proposal, the point says it is a recommendation. emanspeaks accepts, changes or marks open each numbered point. Nothing in it is decided until he accepts it.

How a mixed expression is typed is P-0018's (#33); how a bare pointer name is read is #26's.

## Motivation

The reference says there is no `bool` and that conditions use `u8`, while the TextMate grammar accepts `true` and `false` (cesiumlang/grammar#2). What a condition may be, what a comparison or `&&` gives, and whether `!5` is 0 are written nowhere (#38). Core SIR already assumes an answer: its comparisons and `land`, `lor` and `lnot` give a `u8` (P-0006, point 22), and its conditions take any integer (P-0006, "Typing is strict").

## Design

Each point names where it comes from: a direction of emanspeaks on #38 (2026-11-22), a direction on another issue, or a recommendation.

### Truth values

1. There is no `bool` type; a truth value is a `u8`. Direction, which did not take the suggestion of a distinct `bool`; the reference's "Type System" says so too. This is the draft rule `lex.predeclared.truth`.
2. `true` and `false` are predeclared constants of type `u8`, with the values 1 and 0. Direction (`lex.predeclared.truth`).

### Conditions

3. The condition of an `if` or a `while` may be an integer, true when it is nonzero, or a pointer, true when it is not null. Direction (`s0.condition`).
4. A floating-point value or an error union in a condition is an error. Direction.
5. The other conditions of the language, the first operand of `a ? b : c` (`lex.op.conditional`) and the condition of a `do` loop, follow points 3 and 4. Recommendation: the direction names `if` and `while` as examples, and nothing calls for a second rule.

### The values of the operators

6. A comparison (`==`, `!=`, `<`, `>`, `<=`, `>=`, and `#==` and `#!=` of `lex.op.address-compare`) gives a `u8`: 1 when it holds, 0 when it does not. Recommendation: it follows from point 1, and core SIR gives exactly this (P-0006, point 22).
7. `!` is logical NOT and `!.` is bitwise NOT. Direction on #15 and #28 (`lex.op.not`).
8. `!a` gives 1 when `a` is false by point 3, and 0 otherwise: `!5` is 0 and `!0` is 1. Recommendation, as point 6; core SIR's `lnot`.
9. `a && b` and `a || b` give 1 or 0, never the value of an operand: `5 && 2` is 1. Each operand is read as a condition (points 3 and 4). Recommendation, as point 6; core SIR's `land` and `lor`.
10. `&&` and `||` evaluate their right operand only when the left one does not decide the result. Decided in #9 (`s0.expr.short-circuit`).

### S0

11. In S0 a condition is an integer, nonzero being true (`s0.condition`), and comparisons, `!`, `&&` and `||` give the `u8` of points 6, 8 and 9. Direction on #38 for the condition; recommendation for the values, which is what stage 0 and core SIR do now.

### The other documents

Once the points above are accepted, these are corrected to match them. These are corrections, not design:

- P-0009, point 22, returns `:~Matrix, bool`; with point 1 that is `:~Matrix, u8`.
- The reference's `u8 is_valid = 1;  // true` can read `u8 is_valid = true;` (point 2).
- The TextMate grammar highlights `true` and `false` as constants, not as values of a type `bool` (cesiumlang/grammar#2).

## Alternatives

- **A distinct one-byte `bool`,** with `true` and `false`, conditions required to be `bool`, comparisons returning it, and explicit conversion to and from integers (the suggestion of #38). Not taken by the direction (point 1), for the stated dislike of extra primitive types.
- **Truthiness for floats and error unions,** nonzero or success being true. Not taken by the direction (point 4).

## Drawbacks

- `f(u8 flag)` and `f(u8 byte)` cannot be told apart by overloading, and `printf("{}", flag)` prints `1` or `0`.
- `a & b` and `a && b` differ only when a value is neither 0 nor 1, a mistake the type checker cannot catch.
- A C `_Bool` holds only 0 or 1, which a `u8` does not promise (open question 3).

## Open questions

1. **A pointer in a condition** (point 3) under dereference by default: by #26 a bare pointer name where no pointer is expected is the value it points at, so `if (p)` would test the pointee. The options: a condition takes a pointer as a pointer, so `if (p)` tests for null and the pointee is tested by a comparison; the explicit operator of #26 marks the pointer, `if (#p)`; or a pointer is tested only by `p #!= null` (`lex.op.address-compare`), which leaves point 3 for pointers to expressions that are pointers by type. The two directions do not settle it together.
2. **Shadowing `true` and `false`:** whether a program may shadow them, as the mathematical built-ins (`lex.predeclared.shadowable`), or never, as `alloc` and `free` (`lex.predeclared.fixed`).
3. **C `_Bool`** in `extern` signatures and struct fields: a `u8` that the program keeps at 0 or 1, which has the size of `_Bool` on the common ABIs; or a type for C interop only, used in `extern` blocks.
4. **`true` and `false` in S0:** whether stage 0 has them. P-0007 expects them to replace the corpus's `const u8 TRUE = 1;`, stage 0 has neither today, and the specification keeps the question open (the open points after `s0.expr.alloc`). They cost stage 0 little, and a constant written by hand works around them as cheaply (`s0.purpose`).

## Where it comes from

- #38 (from the design audit of 2026-11-16): the missing semantics, and the suggestion that was not taken.
- 2026-11-22, on #38 (recorded 2026-11-23): no `bool`, `true` and `false` as `u8`, conditions on integers and pointers, no floats or error unions in a condition (points 1 to 4); the values of the operators, the pointer in a condition, overloading and printing, and C `_Bool` left to the proposal.
- 2026-11-22, on #15 and #28: `!` as logical NOT and `!.` as bitwise NOT (point 7).
- 2026-11-22, on #26: the expected type decides how a pointer name is read (open question 1).
