+++
id = "P-0018"
type = "proposal"
status = "proposed"
+++

# Proposal P-0018: expression typing

This proposal decides how an expression gets its type: from its destination, the type of a literal with no context, arithmetic on operands of different types, the implicit conversions, and how overload resolution meets all of these. It carries the design issue #33, with the answers emanspeaks gave there on 2026-11-22 and the rules of the original design that the reference page "Type System" records. Where the issue leaves a point to the proposal, the point says it is a recommendation. emanspeaks accepts, changes or marks open each numbered point. Nothing in it is decided until he accepts it.

The spelling of a conversion (`u8(x)`) and the meaning of `as` are #34's; the values of comparisons and of the logical operators are P-0019's (#38); the forms of literals are #56's.

## Motivation

Three rules of September 2025 (#16) do not compose: a literal takes its type from its context, mixed arithmetic is typed by its destination, and an overloaded function is chosen by the types of its arguments. In `f(a + b)`, `f(255)` or `a + b < c` there is no destination: the argument types are needed to pick the overload, and the overload to type the arguments (#33). The type checker of `cesium1` needs one answer, and core SIR checks types bottom-up and strictly (P-0006, point 24 and "Typing is strict"), so the frontend has to know every conversion it inserts.

## Design

Each point names where it comes from: a direction of emanspeaks on #33 (2026-11-22), a rule of the original design as the reference records it ("Rules from the Original Design" of "Type System" and "Functions"), or a recommendation.

### The destination

1. An expression whose operands have different numeric types takes its type from its destination: in `u8 k = i + j;` both operands are converted to `u8` and the sum is computed as a `u8`. Original design (#16); kept by the direction, which did not take typing bottom-up. This is the draft rule `s0.expr.typing`.
2. The initializer of a declaration and the right side of an assignment have the declared type of the variable or place as their destination. Original design (the reference's examples and #16).
3. Where an expression has no destination, as an argument or an operand of a comparison, the program states the type with a conversion, such as `u8(x)` (`s0.expr.convert`, #34). Direction.

### Literals

4. A literal takes its type from its context, the destination or the operand beside it, and has no type suffix. Decided in #16 (`lex.lit.int.type`, `lex.lit.int.suffix`).
5. An integer literal with no context is an `i32` if its value fits one, and an `i64` otherwise. Direction (`lex.lit.int.type`).
6. A floating-point literal with no context is an `f64`. Direction (`lex.lit.float.type`).

### Mixed arithmetic

7. In arithmetic on two integers of different widths, the narrower operand is promoted. Direction.
8. Arithmetic on a signed and an unsigned operand converts implicitly; it is not an error. Direction, which did not take the suggestion to reject it unless one side is a literal.
9. A comparison of a signed and an unsigned value is mathematically correct, not a comparison of bits: a `u32` holding 3,000,000,000 is greater than an `i32` holding 10, and any negative value is less than any unsigned value. Original design.

### Implicit conversions

10. An integer converts implicitly to a wider integer of the same signedness (`i32` to `i34`, `u32` to `u64`), and a float to a wider float (`f64` to `f128`). Original design.
11. Every integer converts implicitly to every floating-point type, including `i32` to `f32` and `i64` to `f64`, which can lose precision. Direction, which did not take the suggestion to allow only exact conversions.
12. A floating-point value becomes an integer only explicitly, through a rounding function such as `trunc` or `round`. Original design (the reference's "Explicit Conversions").
13. The implicit promotions apply to function arguments as they do to assignments. Original design.

### Overloads

14. An overloaded function is chosen by the types of its arguments: an exact match first, then promotions; the most specific overload wins, and an ambiguous call is an error. Original design (the reference's "Functions").
15. The type of an argument is found from the argument alone, with no destination (point 3), and overload resolution then uses it: a literal argument has its type without context (points 5 and 6), and a mixed expression is converted by the program, as in `f(u8(a + b))`. Recommendation: the issue leaves the circularity to the proposal, "probably by requiring an explicit type there", and this breaks it in one direction only, from the arguments to the overload.

### S0

16. Stage 0 inserts the conversions these rules imply, so that the core SIR it produces keeps both operands of an operation at one type (P-0006, "Typing is strict"). Recommendation: core SIR is typed bottom-up (P-0006, point 24), so the conversions have to be explicit in it.

### The reference page

Once the points above are accepted, the reference pages are corrected to match them. These are corrections, not design:

- "Strong typing with minimal implicit conversions" (overview) no longer describes the language, by points 8 and 11; the issue's direction notes it.
- "use `as` to force" a literal's type, and "`u8 k = i + j` behaves as `i + (j as u8)`" (type system) become `u8(255)` and `u8(i) + u8(j)`, since `as` is bit reinterpretation (#34).

## Alternatives

- **Typing bottom-up from the operands,** with the destination affecting only literals (the suggestion of #33). Not taken by the direction: destination typing stays (point 1).
- **Mixed signedness as an error** unless one side is a literal. Not taken by the direction (point 8).
- **Only exact int-to-float conversions** (`i32` to `f64`, not `i32` to `f32` or `i64` to `f64`). Not taken by the direction (point 11).
- **Type suffixes on literals,** as `255u8` in Rust. Not taken in #16 (point 4).

## Drawbacks

- The same expression can compute different values in different places: `(a + b) / 2` gives one result where its destination is a `u8` and another where it is a `u32`.
- Point 11 loses precision silently (`i64` to `f64` above 2^53), while the opposite direction needs an explicit rounding function (point 12).
- Points 3 and 15 put conversions in calls and comparisons that a typing bottom-up would not need.

## Open questions

1. **Mixed types with no destination:** whether such an expression is an error the program fixes with a conversion (point 3), or typed by promoting the narrower operand (point 7), so that `u8 + u32` is a `u32` in an argument. The two directions read either way.
2. **Promotion under a destination:** whether, in `u8 k = x + y;` with a `u32` operand, the operands are converted to `u8` first (point 1) or promoted to `u32`, computed, and then narrowed. The low bits agree for `+`, `-` and `*`, not for `/`, `%`, `>>` or a comparison inside the expression.
3. **Mixed signedness at equal width** (`i32 + u32`) with no destination: its result type, signed or unsigned, or one width wider (`i64`).
4. **Which contexts are destinations** beyond point 2: a `return` value (the function's return type), the argument of a function with one signature (its parameter type, which point 13 suggests and point 3 does not), the operands of a compound assignment, an index.
5. **Implicit narrowing of one value:** whether `u8 k = x;` with a `u32` `x` narrows implicitly, as point 1 does in `u8 k = x + y;`, or needs `u8(x)`, the narrowing form of #34.
6. **The type of `a + b < c`:** what types the operands of a comparison are computed in, when the comparison's own value is a `u8` (P-0019).
7. **Literals in generic calls** such as `max(3, x)`: whether the literal takes the type of the other argument, as an operand beside it does (point 4), or its type without context (point 5) before `T` is inferred (#29).
8. **A literal with no context that fits no `i64`,** such as 18446744073709551615: an error, or a `u64`; and a literal that does not fit its destination (#56).
9. **Arbitrary widths** such as `u34` and `i34` (the reference's "Type System"): how they are stored, how they wrap, how they cross the C ABI, and whether their names are keywords or predeclared (#50).
10. **Float to integer by `i32(x)`:** whether the conversion form of #34 also converts a float to an integer, and how it rounds, beside the rounding functions of point 12.

## Where it comes from

- #16: the rules of September 2025, as the reference pages "Type System" and "Functions" record them (points 1, 2, 4, 9, 10, 12 to 14).
- #33 (from the design audit of 2026-11-16): the three rules that do not compose, and the suggestion that was not taken.
- 2026-11-22, on #33 (recorded 2026-11-23): destination typing stays, literals with no context, mixed arithmetic, all int-to-float conversions implicit (points 1, 3, 5 to 8 and 11); the overloads, mixed types and arbitrary widths left to the proposal.
- 2026-11-22, on #34: the conversion `u8(x)`, and `as` as bit reinterpretation.
