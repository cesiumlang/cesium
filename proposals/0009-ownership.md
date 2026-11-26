+++
id = "P-0009"
type = "proposal"
status = "proposed"
+++

# Proposal P-0009: ownership, borrowing and lifetimes

This proposal decides how Cesium tracks ownership and borrowing: the pointer forms, owners and their release, `free`, `destruct`, `null` and `undefined`, ownership across function calls, methods, address arithmetic and `defer`. It carries the decision record cesiumlang/cesium#11, which was guidance and not accepted: the record calls itself decided on 2026-11-16, from decisions emanspeaks made round by round in #12 and #17, and under P-0000 it is guidance until a proposal is accepted. emanspeaks accepts, changes or marks open each numbered point. Nothing in it is decided until he accepts it.

## Motivation

The record replaces the ownership sketch of the September 2025 design chat (`Matrix m := create_matrix()`, `move()`, `#m` and `~m` at every call site, function-scoped `defer`). It consolidates the rounds of #12 and #17 (moved from cesiumlang/cesiumlang.org#1 and #6) and supersedes the individual comments there, including the answers that were later reversed (see Alternatives).

The model in one paragraph, as the record gives it: ownership and borrowing are properties of pointers only. A plain value is just a value. A pointer either borrows the memory it points at or owns it, and separately either may or may not modify the data through itself. The compiler tracks, for every owning pointer, whether it currently owns something, and requires every owned object to be explicitly freed, moved or returned on every path. There is no garbage collector, no reference counting, no lifetime annotations and no run-time ownership bookkeeping beyond the pointer's own value.

## Design

The points are the record's rules and keep its numbers: point N is rule N of the record, which the specification cites as "rule N".

The pointer forms:

| Declaration | Kind | Data through the pointer |
| --- | --- | --- |
| `#T p #= q;` | immutable borrow | read-only |
| `~T p ~= q;` | mutable borrow | read-write |
| `#T p := f();` | owner | read-only |
| `~T p := f();` | owner | read-write |

### Pointer forms

1. `#` and `~` in the type say whether the data can be modified through that pointer. They do not restrict re-pointing.
2. `#=`, `~=` and `:=` are address-context operators: they set where the pointer points. `#=` is an immutable borrow, `~=` a mutable borrow, `:=` takes ownership.
3. The sigil and the operator must agree. `~T p #= q;` and `#T p ~= q;` are compile errors.
4. `:=` is only meaningful with a pointer on the left. `Matrix m := f();` is a compile error.
5. Plain `=` on a pointer assigns through it (dereference by default), so it is never how a pointer is initialized: `#i32 p := alloc(100);`, not `#i32 p = alloc(100);`.
6. A borrow may be re-pointed with a later `#=` or `~=`.

### Owners

7. Only the owner may `free`. An immutable (`#`) owner can free. A borrower never can.
8. In-place modification of owned data needs a `~` owner. Taking a mutable borrow from a `#` owner is a compile error.
9. Every owner must be released. An owner that reaches the end of its scope without being freed, moved or returned is a compile error. This is stricter than Rust, which frees automatically at scope end.
10. Reassigning an owner that still owns is a compile error. The old memory must be freed or moved first.
11. Move: `#T b := a;` where `a` is an owner transfers ownership to `b`. Using `a` afterwards is a compile error.
12. Mutability on transfer: the receiver may be more restrictive than the source, never less. A `~` owner can be received as `#T`; a `#` owner cannot be received as `~T`.

### `free`, `destruct`, `null` and `undefined`

13. `free(p)` runs the type's `destruct` function, releases the memory, and resets `p` to `undefined`.
14. `destruct` is symmetric: for a plain (non-pointer) value it runs when the value goes out of scope.
15. `undefined` is a keyword. For a pointer it is a defined "owns nothing" state: null in optimized builds, optionally a distinct trap sentinel in debug or sanitizer builds. For a plain value it means truly uninitialized.
16. `null` and `undefined` are different: `null` is assigned deliberately; `undefined` means never assigned, or freed.
17. `free` on a `null` or `undefined` pointer is a defined no-op.
18. A `free` that the compiler can prove is redundant (for example a second `free` in straight-line code) is still a compile error. The no-op covers only what the compiler cannot decide.
19. There is no test for `undefined` (following Zig, which as far as the record knows provides none).

### Functions

20. Arguments are type-checked strictly. A pointer is passed bare: `f(p)`. A plain value needs its address taken explicitly: `f(#x)` for a `#T` parameter, `f(~x)` for a `~T` parameter. Bare `f(x)` with a plain `x` is a compile error. The same holds in assignments: `p #= #x;` for a plain `x`, `p #= q;` for a pointer `q`.
21. Passing anything that is not mutable to a `~T` parameter is a compile error. A `~T` pointer may be passed to a `#T` parameter.
22. Owned return values are marked per value with `:` in front of the sigil: `:#T` or `:~T`. This allows mixed returns: `:~Matrix, bool = f() { ... }`.
23. A function that returns a plain `#T` or `~T` returns a borrow, and that borrow must derive from its parameters or from globals.
24. Owned parameters: `:#T` or `:~T` on a parameter means the function takes ownership of the argument.
25. Owning struct fields use the same spelling: `:#T` or `:~T`.

### Methods

26. An unprefixed method takes `this` as a mutable borrow and cannot be called through a `#` pointer.
27. A read-only method is declared with `#` before its name: `f64 = #distance_from_origin() { ... }`.

### Pointer arithmetic and contexts

28. Address space and data space are separate; the rule is not to mix them.
29. Ordinary operators never do address arithmetic. `a + p` with a pointer `p` auto-dereferences `p` and yields a number. Assigning that number to a pointer-typed variable is a compile error.
30. Address arithmetic uses decorated operators. Both families exist (`#`-decorated and `~`-decorated, such as `#+` and `~+`), including in-place forms such as `p#++`. They are only valid in a pointer context; both kinds of borrow are pointer contexts.
31. Advancing a mutable borrow is a request to borrow the new address.
32. The result of a `~`-decorated operation may be assigned to an immutable pointer (the receiver is more restrictive).

### `defer`

33. `defer` is block-scoped: a deferred statement or block runs when its enclosing block exits, in LIFO order.
34. The deferred statement is evaluated when it runs (Zig convention), so it sees the values variables hold at that moment.
35. The compiler checks a deferred statement as if it were written at every exit from the block. A deferred `free(p)` on a pointer that provably owns nothing at that point is a compile error; one that might own nothing is allowed and is a no-op at run time (point 17).
36. In a `with` block, a `defer` in the body runs at the end of the body, before the context `exit`.

### Examples

The record's examples, unchanged:

```cesium
// Returns ownership of a mutable Matrix.
:~Matrix = create_matrix(i32 rows, i32 cols) {
    ~Matrix m := alloc(1);
    // ... initialize ...
    return m;                         // ownership leaves with the return value
}

f64  = determinant(#Matrix m) { ... }        // immutable borrow
void = transpose_inplace(~Matrix m) { ... }  // mutable borrow
void = consume(:#Matrix m) { free(m); }      // takes ownership

void = demo() {
    ~Matrix a := create_matrix(3, 3);
    defer free(a);                    // runs when this block exits

    f64 d = determinant(a);           // pointer passed bare; ~ to # is allowed
    transpose_inplace(a);

    #Matrix b := create_matrix(2, 2); // received as immutable: allowed
    consume(b);                       // b is moved; using b after this is an error

    Matrix v;                         // plain value
    f64 dv = determinant(#v);         // plain value: address taken explicitly
    transpose_inplace(~v);
}
```

Keeping something alive past a `with` block:

```cesium
void = load() {
    ~Buffer buf;                      // owns nothing yet (undefined)
    defer free(buf);                  // no-op if buf was never assigned

    with f = file("data.bin") {
        if (f.size == 0) { return; }  // early exit: deferred free is a no-op
        buf := read_all(f);
    }
    // buf is still valid here
}
```

Reassigning an owner:

```cesium
~Matrix m := create_matrix(3, 3);
defer free(m);                        // frees whatever m owns at block exit
// m := create_matrix(4, 4);          // error: m still owns the 3x3 matrix
free(m);
m := create_matrix(4, 4);             // fine; the defer will free this one
```

### What the record says needs updating

As the record lists it, on 2026-11-16:

- The language reference (cesiumlang/cesiumlang.org): `type-system.md`, `variables-constants.md`, `functions.md`, `memory-management.md`, `object-oriented.md`, `operators-expressions.md`, and `lexical-structure.md` (the new keyword `undefined`); the line-level list is in cesiumlang/cesiumlang.org#1.
- The grammar (cesiumlang/grammar#2): `~=`, `~`-decorated operators, `:#T` and `:~T` types, `#name` method declarations, `undefined`.
- The spelling dictionary (cesiumlang/cspell-cesium#1): `undefined`.
- The stage 0 design (#9): owning pointers there are initialized with plain `=` (`#Token t = alloc(1);`); under point 5 these become `:=`. Stage 0 still parses the ownership forms without enforcing them.

## Alternatives

The record gives the earlier positions it reversed, each with the position that replaced it:

| Earlier position | Final position |
| --- | --- |
| Sept 2025: `Matrix m := create_matrix();` owns a value | Ownership is pointer-only; that line is an error |
| Sept 2025: `move(x)` for explicit transfer | `b := a;` moves; no `move()` |
| Sept 2025: `#m` / `~m` at every call site | Sigil only when taking the address of a plain value |
| Sept 2025: `defer` runs at function `return` only | `defer` is block-scoped |
| "No `~=` operator" (assistant inference, round 1) | `~=` is the mutable-borrow operator |
| Round 2: ownership-returning function declared with `:=` | `:#T` / `:~T` on each owned return value |
| Round 2: a moved-from owner becomes a borrow | Use after move is a compile error |
| Round 2: a move cannot change mutability | Downgrade allowed, upgrade never |
| Round 2: an owner's defers run before it is reassigned | Dropped; reassigning while owning is an error |
| Round 3: decorated operators only on the right of `~=` | Valid in any pointer context |
| Rounds 3-4: object-bound `defer` that travels with ownership | Dropped (would need run-time storage) |

## Drawbacks

The record names none.

## Open questions

As the record lists them. Questions 1 to 5 should be settled before the reference is rewritten; 6 to 12 can wait.

1. **Borrow exclusivity and lifetime.** The September 2025 rule was "any number of immutable borrows, or one mutable borrow, never both", with scope-based lifetimes. It was not revisited on 2026-11-16. Confirm it still holds, and state: when a borrow ends (when the borrowing pointer leaves scope or is re-pointed?), whether the owner may be read while a mutable borrow is live, and that freeing or moving an owner with live borrows is an error.
2. **Copying a value that owns memory.** "Plain values are just values and `=` copies" is fine for a struct of numbers. For a value whose type has a `destruct`, or has `:#T` fields, or is a built-in container (`list`, `dict`, `str`), a field-by-field copy would produce two values that both free the same memory. Is `=` on such a value a deep copy, a move, or an error unless the type defines copying?
3. **Paths that disagree.** If an owner is released on one branch and not on another, its state after the branches rejoin is "might own". Point 17 makes `free` safe there. Still to state: is using a might-be-`undefined` pointer a compile error, and is reaching the end of scope in a "might own" state a compile error unless a `free` covers it?
4. **Moves at call sites.** Passing an owner to a `:#T` or `:~T` parameter moves it, and by point 20 it is written bare: `consume(b)`. Nothing at the call site shows that `b` is gone afterwards. Accept that, or mark it?
5. **Declaring an unassigned owner.** Is it `~Buffer buf;` (implicitly `undefined`), or must it be written out, and with which operator?
6. Borrows stored in struct fields or containers (no lifetime annotations to check them against).
7. Owners held in arrays or containers, where the element is chosen at run time.
8. Moving an owning field out of a struct.
9. Owners at global scope: exempt from the must-release rule?
10. Pointers that come from C (`extern` declarations): owned, borrowed, or outside the rules. cesiumlang/cesium#4 (ADR 0001) defers an "unsafe mode" for crossing the C boundary.
11. Ownership with multi-level pointers (`##T`, `~#T`).
12. How `alloc`, `realloc` and `copy` are typed under these rules (`alloc` returns ownership; `realloc` consumes and returns it), and whether allocation failure is an error value or `null` (#17).

## What changed since the record

The record has no comments, so it has no amendments.

Later material that touches it, without changing its text: the amendments of #9 (2026-11-16 and 2026-11-17) apply points 1, 5 and 9 to S0 and record where stage 0 and the S0 sample fall short of them; those points are carried by P-0007 and tracked in #74.
