+++
id = "P-0016"
type = "proposal"
status = "proposed"
+++

# Proposal P-0016: dereference by default and the expected type

This proposal decides what a bare pointer name means where the context does not fix it: the expected type, the address operator `#p`, overloads on `T` and `#T`, address comparison with `#==` and `#!=`, generics, containers, printing, `return`, pointer fields and multi-level pointers. It carries the design issue #26, with the answers emanspeaks gave there on 2026-11-22 and the decisions of the ownership record #11 (P-0009) that it builds on. Where emanspeaks left a point to the proposal (2026-11-22), the point says it is a recommendation. emanspeaks accepts, changes or marks open each numbered point. Nothing in it is decided until he accepts it.

Ownership and borrowing are P-0009's; null, optional values and uninitialized data are #43's; overload resolution with no destination type is #33's; generics are #29's.

## Motivation

A pointer is read through by default and there is no dereference operator (`s0.expr.deref`; P-0009, point 5). That works where the context fixes what is expected, as in `f(p)` to a `#T` parameter or `x = p + 1`, and leaves every other context undefined (#26, items 1 to 8): overloads, generics, comparison and null tests, containers, variadics, pointer fields, multi-level pointers and `return`. The grammar, S0 and the semantic IR all depend on one rule for these.

## Design

### The rule

1. The expected type decides: where the context expects a pointer, a pointer name is the pointer; where no expected type says so, a bare pointer name is the value it points at, and its address needs an explicit operator. Direction; the draft rule `s0.expr.expected`.
2. That operator is `#`: for a pointer `p`, `#p` is the address `p` holds, the "address held in `p`" form #26 asked for; for a plain value `x`, `#x` is its address, as before (`lex.op.sigils`). Direction, as this proposal reads it: the direction writes `#p` for "the address", which is taken here as the address `p` holds, not the address of the variable `p` itself (open question 7).
3. There is still no dereference operator. Decided in #9 (amendment of 2026-11-16) and #11, rules 5 and 29 (`s0.expr.deref`).

### Overloads (item 1)

4. `f(T)` and `f(#T)` may both be defined, and the argument expression chooses: `f(p)` passes the value and calls `f(T)`, `f(#p)` passes the address and calls `f(#T)`. Direction.
5. Where only `f(#T)` is defined, the parameter is the expected type, so `f(p)` passes the pointer, and `f(#p)` passes the same address. Decided in #11, rule 20 (P-0009, point 20; `s0.check.pointer-argument`), with point 2.

### Comparison and null (item 3)

6. `#==` and `#!=` compare addresses, and a pointer is tested against null with `p #== null`; `p == q` compares the values the pointers point at. Direction; the draft rule `lex.op.address-compare`.
7. `#==` and `#!=` have the precedence of `==` and `!=`. Recommendation: obvious, since they are those operators decorated with `#`, as `#+` is `+` (`lex.op.decorated`).

### Generics, containers, printing and `return` (items 2, 4, 5 and 8)

8. A generic parameter `T` fixes no expected type, so `T` is inferred from the argument expression as point 4 chooses an overload: with `#i32 p` and `#i32 q`, `swap(p, q)` infers `T = i32` and passes the values, and `swap(#p, #q)` infers `T = #i32`. Inside the function, `a = b` means what `=` means for `T`; for a pointer type it assigns through (P-0009, point 5). Recommendation (left to the proposal): one rule for overloads and generics.
9. A container's element type is the expected type of the element it takes: `l.append(p)` stores the pointer in a `list[#T]` and a copy of the value in a `list[T]`. Recommendation (left to the proposal): it is point 1 with the parameter's type.
10. A variadic argument has no expected type: `printf("{}", p)` prints the value and `printf("{}", #p)` the address. Recommendation (left to the proposal): it is point 1.
11. `return e;` has the function's return type as its expected type: `return p;` returns the address in a function that returns `#T`, `~T`, `:#T` or `:~T`, and the value in one that returns `T`. Recommendation (left to the proposal): #26 calls this fine as it is; it is point 1 with the return type.

### Pointer fields (item 6)

12. Plain `=` on a pointer field assigns through it, as on any pointer: `t.start = "hello";` writes to where `t.start` points. The field is pointed elsewhere with an address-context assignment, `t.start #= "hello";`. Decided in #9 (amendment of 2026-11-16) and #11, rules 2 and 5 (`s0.assign.pointer`).

### Multi-level pointers (item 7)

13. For `##T pp`, bare `pp` is the `T` at the end of both levels, and `#pp` is the address `pp` holds (point 2). The `#` operator takes a level count, so that the middle level, the `#T` that `pp` points at, is reachable. Direction.
14. Where the expected type says the level, no count is written: `#T mid #= pp;` takes the `#T` that `pp` points at. Recommendation: point 1 says the expected type decides, and it then decides how many levels are followed.
15. The level count is an integer literal after `#`, the number of levels followed before the address is taken: `#1 pp` is the `#T` that `pp` points at. Recommendation (the spelling was left to the proposal): it needs no new token, and `#` in front of a literal has no other meaning, since a literal has no address. Its alternatives are open question 1.

### S0

16. S0 has no overloads, generics, containers or variadic Cesium functions, so its rule is points 1 to 3, 5, 11 and 12 (`s0.expr.expected`). Consequence of the points above.
17. The level count is not in S0: stage 1 reaches the elements of `argv` by indexing (`g_argv[i]`, `bootstrap/stage1/loc.csm`) and needs no middle level. Recommendation by `s0.purpose`.
18. In the mapping to core SIR (`s0.sir`), `#p` of a pointer `p` is the value of `%p`, not `addr(%p)`, which is the address of the variable `p` itself; `#x` of a plain value stays `addr(%x)`. Consequence of point 2.

### The reference pages

Once the points are accepted, these follow; they are corrections, not design:

- `if (numbers == null)` becomes `if (numbers #== null)` (point 6).
- The operator list and the precedence table of "Operators and Expressions" gain `#==` and `#!=` (point 7; #15).
- The open note on #26 in "Type System" goes, and the page states points 1, 2 and 13.

## Alternatives

- **Make the overload pair `f(T)` and `f(#T)` an error** (suggestion (c) of #26). Not taken by the direction (point 4).
- **Built-ins `same(p, q)` and `isnull(p)`** in place of decorated comparisons (suggestion (b)). Not taken: the direction keeps the rule that `#` decorates the operator (point 6).
- **A dereference operator,** as C's `*p`, so that a bare name is always the pointer. Not taken: dereference by default was confirmed by the amendment of 2026-11-16 on #9 (point 3).
- **Repeating the sigil for the level count,** `##pp`, by analogy with the type `##T`. Not recommended: point 2 already gives `#pp` a meaning, the outermost address, so `##pp` would read as the address of that address rather than a count.

## Drawbacks

- What a bare name means depends on where it stands: moving `p` from a `#T` declaration into a `printf` argument changes it from the pointer to the value.
- Adding `f(T)` next to an existing `f(#T)` silently changes what every `f(p)` passes and calls (points 4 and 5).
- `#` gives the address a pointer holds and the address of a plain variable; the address of a pointer variable itself, a `##T`, needs a declaration with an expected type or a level count.

## Open questions

1. **The spelling of the level count** (point 15): `#1 pp`, a count in brackets such as `#[1]pp`, or another; and whether `~` takes a count the same way, which waits on ownership with multi-level pointers (P-0009, open question 11).
2. **An overload set where no candidate takes the value:** with `f(#T)` and `f(#U)` both defined and `#T p`, whether `f(p)` passes the pointer, as point 5 does for one function, or is an error that asks for `f(#p)`.
3. **`#==` on multi-level pointers:** which level `pp #== qq` compares for `##T` operands, and whether `#p == #q` means the same as `p #== q`.
4. **A pointer in a condition:** `if (p)` tests non-null by the direction of #38, while point 1 makes a bare `p` with no expected type its value; whether a condition expects a pointer, or `if (p #!= null)` is required (#38).
5. **`null`, `#==` and `#!=` in S0** (#43): stage 1 writes `((#f[0]) as uword) == 0` around them today (`bootstrap/stage1/loc.csm`).
6. **The September 2025 contexts:** whether `()` as a value context inside an address context and `addrcontext()` stay, now that `#p` and the level count say the address explicitly (#15, item 6).
7. **The reading of point 2:** whether `#p` of a pointer is the address `p` holds, as read here, or the address of the variable `p`, a `##T`. Points 13, 15 and 18, and P-0017's point 13, follow the reading of point 2 and change with it.

## Where it comes from

- #26 (the design audit of 2026-11-16): items 1 to 8 and the suggestions (a) to (d).
- 2026-11-22, on #26 (recorded 2026-11-23): the rule of the expected type and `#p` (points 1 and 2), overloads on `T` and `#T` (point 4), `#==` and `#!=` (point 6), and the level count (point 13); generics, containers, printing, pointer fields and `return` left to the proposal (points 8 to 12), and the spelling of the level count (point 15).
- #9 (amendment of 2026-11-16) and #11, rules 2, 5, 20 and 29 (P-0009): dereference by default, no dereference operator, pointer arguments and pointer fields (points 3, 5 and 12).
