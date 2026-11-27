+++
id = "P-0017"
type = "proposal"
status = "proposed"
+++

# Proposal P-0017: numeric conversion and `as`

This proposal decides how a value is converted to another type: numeric conversion written with the type name as a function, `u8(x)`, and `as` as bit reinterpretation and pointer cast. It carries the design issue #34, with the answers emanspeaks gave there on 2026-11-22 and the rules of the original design that the reference page "Type System" records. Where a point follows from the answers but was not asked, it says it is a recommendation. emanspeaks accepts, changes or marks open each numbered point. Nothing in it is decided until he accepts it.

Implicit conversions and the typing of expressions are #33's; constructors of user types are #18's; the floating-point policy is #35's; what a bare pointer name means is P-0016's.

## Motivation

`as` had three meanings (#34): bit reinterpretation between types of the same size in the reference and the September 2025 chat, numeric conversion in the stage 0 design (#9) and the semantic IR (#8), and union member extraction in the reference's pattern matching. Under the first there is no way to narrow a `u64` to a `u8`, to widen explicitly, or to change signedness, and the stage 0 sample cannot be written. Since #33 makes the program state the type where an expression has no destination, the explicit conversion is needed in ordinary code too.

## Design

### Numeric conversion

1. A number is converted to another numeric type by the type's name used as a function: `u8(x)`, `u64(t.len)`. The conversion widens, narrows, changes signedness, and converts an integer to a float. Direction; the draft rule `s0.expr.convert`.
2. Widening keeps the value: an unsigned source is extended with zeros, a signed one with its sign. Recommendation: "widen" keeps the value, and core SIR widens by the source's signedness (P-0006, point 23).
3. Narrowing truncates silently: `u8(x)` of a wider value keeps its low bits (wraparound). Direction.
4. A change of signedness at the same width keeps the bits: with `i32 i = -1`, `u32(i)` is 4294967295. Recommendation: it is the wraparound of point 3 at equal width, and what core SIR's `as` does.
5. `T(x)` means core SIR's `as(T, x)`, which fills the missing row of the mapping (`s0.sir`). Recommendation: P-0006, point 23 gives `as(T, v)` exactly the rules of points 2 to 4.
6. Implicit conversions stay as #33 has them: an expression takes its type from its destination, and where it has none (an argument, a comparison) the program says the type with `T(x)`: `f(u8(a + b))`. Direction in #33 (`s0.expr.typing`).
7. A conversion takes one argument, and a pointer argument is read through: with `#u32 p`, `u64(p)` converts the `u32` that `p` points at. A pointer is converted to another type only with `as` (points 11 and 13). Recommendation: the argument of a conversion expects no pointer, so by P-0016, point 1 a bare pointer name is its value.

### Telling `T(x)` apart from a call

8. `T(x)` is parsed as a call. Name resolution makes it a conversion when the name is a numeric type, and a constructor call when it is a type the program defines (#18). Recommendation: the parser needs no list of type names (as P-0013, point 6), and the rule holds whether the sized type names are keywords or predeclared names, which is open (#50).

### `as`

9. `as` reinterprets the bits of a value as another type of the same size: `bits as f32`. Direction, and the original design.
10. `as` between types of different sizes is an error. Direction ("between same-size types").
11. `as` converts a pointer to another pointer type: `q as #u8`. Direction, and the original design (`q as #T`).
12. The target of `as` is the expected type of its operand: with a pointer target, a pointer operand is the pointer; with any other target, it is the value the pointer points at. Recommendation: it is P-0016, point 1 applied to `as`.
13. A pointer and a `uword`, which is pointer-sized (P-0006, point 15), reinterpret into each other with `as`: `#p as uword` is the address `p` holds as a number, and `n as #u8` a pointer to that address. Recommendation: it follows from points 9 and 12 and P-0016, point 2, and core SIR converts between pointers and `uword` (P-0006, point 23).

### S0

14. S0 has `T(x)` with its integer types, and `as` between its integer types of the same size, between pointer types, and between a pointer and a `uword`. In the grammar (`s0.expr`), `primary` gains the conversion of an integer type name followed by `(`, an expression and `)`, and `cast` keeps `as type`. Recommendation: stage 1 needs both, as `s[i] as u64` (a widening, `bootstrap/stage1/lex.csm`) and `(#f[0]) as uword` (an address, `loc.csm`) show; S0 has no floats (`s0.excluded`).

### The stage 0 sources and the reference pages

Once the points are accepted, these follow; they are corrections, not design:

- The stage 0 sample of #9 (P-0007): `('0' + (v % 10)) as u8` becomes `u8('0' + (v % 10))`, and `t.len as u64` becomes `u64(t.len)`.
- The S0 mapping (`s0.sir`) gains `T(x)` as `as(T, x)`, and its row `e as T` keeps only the reinterpretations of point 14.
- Stage 0 and stage 1 convert numbers with `T(x)` and reject `as` between sizes that differ; the row of `s0.expr.convert` in "Stage 0 as implemented" goes.
- "Type System", "Rules from the Original Design": "use `as` to force one" (the type of a literal) becomes `T(x)`, as `u8(255)`.

## Alternatives

- **`as` as numeric conversion** (the suggestion of #34), as stage 0 and core SIR assume and as Rust and Zig programmers expect. Not taken: `as` stays bit reinterpretation (point 9).
- **A built-in `bitcast(T, x)`** for bit reinterpretation (the same suggestion). Not taken, since `as` keeps that meaning.
- **Narrowing that traps or returns an error** when the value does not fit (item 2 of #34). Not taken: narrowing truncates silently (point 3); a checked form is open question 2.

## Drawbacks

- Readers who know Rust or Zig read `x as u8` as a value conversion; here it is an error unless `x` is one byte.
- `T(x)` looks like a call and, for a user type, is a constructor call (#18): what it does depends on what the name resolves to.
- Narrowing loses high bits without a warning, and there is no checked form yet.
- A change of signedness at the same width has two spellings with the same result, `u32(i)` and `i as u32`.
- Stage 0, stage 1, the sample of #9 and the SIR mapping all use `as` for numeric conversion today, and each changes.

## Open questions

1. **Union member extraction** (item 4 of #34), which the reference writes `num as i32`: a third meaning of `as`, binding in `switch` and `catch` only (the suggestion of #34), or another spelling.
2. **A checked narrowing:** whether there is one, and its form (a built-in that returns an error union, or a trap).
3. **Float to integer and float to float:** whether `i32(f)` converts, with which rounding (out of range it has one defined result, P-0006, point 12), or float to integer goes only through `floor`, `ceil`, `round` and `trunc`, as the original design has it; and whether `f32(d)` narrows a float.
4. **Int to float that is not exact,** such as `f32(x)` of a large `i64`: the rounding, with the floating-point policy (#35).
5. **The type of the argument:** whether `T(x)` is a destination for `x` (#33), so that `u8(a + b)` computes in `u8`, or `x` is typed without one and then converted; and whether `u8(300)` is an error or 44.
6. **Bit reinterpretation in core SIR:** `as(T, v)` converts integers and floats by value (P-0006, point 23), so `bits as f32` needs an operation core SIR does not have, or a store and a load through memory. S0 has no floats, so stage 0 does not need it.
7. **The precedence of `as`,** which the reference's table does not give; the place in `s0.expr` is provisional (#15).

## Where it comes from

- #34 (the design audit of 2026-11-16): the three meanings of `as` and items 1 to 4.
- 2026-11-22, on #34 (recorded 2026-11-23): `as` stays bit reinterpretation between types of the same size; numeric conversion is `T(x)`; narrowing truncates silently; pointer casts use `as` (points 1, 3, 9 to 11); union member extraction and a checked narrowing open.
- 2026-11-22, on #33: destination typing, and `T(x)` where there is no destination (point 6).
- The reference page "Type System", "Rules from the Original Design": `as` is bit reinterpretation only, and `q as #T` (points 9 and 11).
- P-0006, points 15 and 23 (#8): `uword` and core SIR's `as` (points 2, 4, 5 and 13).
