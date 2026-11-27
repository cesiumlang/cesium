+++
id = "P-0014"
type = "proposal"
status = "proposed"
+++

# Proposal P-0014: the remaining operators and the precedence table

This proposal completes the operators: unary `+` and `-`, `++` and `--`, compound assignment, the conditional operator `?:`, the elementwise product `.*`, unary `..`, the comma, and the full precedence table. It carries the design issue #15, with the answers emanspeaks gave there on 2026-11-22. Where the answers leave a point to the proposal, the point says it is a recommendation. emanspeaks accepts, changes or marks open each numbered point. Nothing in it is decided until he accepts it.

Logical and bitwise NOT (`!`, `!.`), the reading of `~` next to an operator and the levels of the decorated operators are P-0012's; the complete list of decorated operators and the level count of `#` stay with #26.

## Motivation

The precedence table of the reference page "Operators and Expressions" has 19 levels and leaves out operators the reference itself uses: unary `-`, `+` and `!` (`-128`, `!file_exists(path)`), `count++` on the control flow page, compound assignment, and the ternary `a > b ? a : b` on the functions page; it says nothing of elementwise products or of what unary `..` does (#15, items 1 to 10). The grammar of the specification (#57) needs every operator at a level, and stage 0 rejects unary minus because the design is open, so S0 source writes `-x` as `0 - x`.

## Design

Each point names where it comes from: a direction of emanspeaks on #15 (2026-11-22), a rule the reference page records, or a recommendation.

### Unary `+` and `-`

1. Unary `+` and `-` are prefix operators: `-x` is the negation of `x`, core SIR's `neg`, which wraps (`s0.expr.arithmetic`), and `+x` is `x`. Direction for their place in the table (#15, item 1: a documentation fix); recommendation for their meaning, which is C's.

`!` and `!.` are P-0012's (points 1 to 4).

### Increment and decrement

2. `++` and `--` are prefix and postfix operators, as in C: `x++` yields `x` and then adds 1 to it, `++x` adds 1 and yields the new value. Direction; the draft rule `lex.op.increment`.
3. On a pointer, the decorated forms move the address: `p#++` after yielding it, `#++p` before. Direction.
4. A plain `p++` on a pointer adds 1 to the value `p` points at. Recommendation: a pointer is read through by default (`s0.expr.deref`; #11, rules 5 and 29).
5. `--` has the same decorated forms, `p#--` and `#--p`. Recommendation: the direction names `++` only, and `--` mirrors it.

### Compound assignment

6. The compound assignment operators are `+=`, `-=`, `*=`, `/=`, `%=`, `^=`, `&=`, `|=`, `<<=`, `>>=`, `**=` and `><=`, at the level of assignment. Direction; the draft rule `lex.op.compound`.
7. `x op= e` means `x = x op e`, with the place `x` evaluated once. With Cesium's operators, `^=` raises to a power, `><=` is exclusive or and `**=` concatenates. Recommendation: C's meaning, with the operators of the table.
8. On a pointer, a compound assignment works on the value it points at, as `=` does (`s0.assign.pointer`). Recommendation: it follows from that rule.

### The conditional operator

9. `c ? a : b` is kept, at one level just above assignment. Direction; the draft rule `lex.op.conditional`.
10. It evaluates `c` as a condition (`s0.condition`) and then only the one of `a` and `b` it chooses; it groups from the right, so `a ? b : c ? d : e` is `a ? b : (c ? d : e)`. Recommendation: C's meaning, which the direction keeps.

### The elementwise product

11. `.*` is the elementwise product, and `*` stays the product that depends on its operands. Direction; the draft rule `lex.op.elementwise`.
12. `.*` is at the level of `*`. Recommendation: it is a product, and MATLAB, where it comes from, puts the two on one level.

### Unary `..`

13. Unary `..`, postfix (`value..`) and prefix (`..array`), keeps its places in the table and is reserved: it means nothing, and a program that uses it is in error, until a proposal says what it does. Direction; the draft rule `lex.op.unary-range`.

### The comma

14. There is no comma operator. A comma only separates arguments, items, initializers, and the targets and types of multiple returns; it has no precedence level. Several updates go in separate statements or a block. Direction; the draft rule `lex.op.no-comma`.

### Pointers

15. `#q[i]` is `#(q[i])`: postfix operators bind tighter than prefix ones, and an array does not decay to a pointer. Decided in the reference (operators-expressions, "Decorated Operators").

### The precedence table

16. The levels, from the highest to the lowest. Operators on one level group from left to right, except exponentiation and the conditional operator, which group from the right; assignment is open question 1. A decorated binary operator is at the level of the operator it decorates (P-0012, point 11). Decided in the reference for the 19 levels it has, with the additions of the points named:

| Level | Operators | Source |
| --- | --- | --- |
| 1 | call `f(a)`, index `a[i]`, member `a.b` | reference |
| 2 | custom unary `$name$x` | reference |
| 3 | postfix: transpose `~`, `++`, `--`, `p#++`, `p#--`, unary `..` (reserved) | reference; points 2, 3, 5, 13 |
| 4 | prefix: `+`, `-`, `!`, `!.`, address-of `#x` and `~x`, root `::x`, `++`, `--`, `#++p`, `#--p`, unary `..` (reserved) | reference; points 1 to 5, 13; P-0012 |
| 5 | binary root `::` | reference |
| 6 | exponentiation `^`, from the right | reference |
| 7 | `*`, `/`, `%`, cross product `@`, `.*` | reference; point 12 |
| 8 | concatenation `**` | reference |
| 9 | `+`, `-` | reference |
| 10 | `<<`, `>>` | reference |
| 11 | `<`, `>`, `<=`, `>=` | reference |
| 12 | `==`, `!=`, `#==`, `#!=` | reference; #26 |
| 13 | `&` | reference |
| 14 | `><` (exclusive or) | reference |
| 15 | `\|` | reference |
| 16 | `&&` | reference |
| 17 | `\|\|` | reference |
| 18 | custom binary `$name$` | reference |
| 19 | conditional `c ? a : b`, from the right | points 9, 10 |
| 20 | assignment `=`, `#=`, `~=`, `:=` and the compound assignments | reference; point 6 |

### The reference page

Once the points above are accepted, the page "Operators and Expressions" is corrected to match them. These are corrections, not design:

- Level 4 gains unary `+`, `-`, `!` and `!.`, and prefix `++`, `--` and `#++p`; level 3 gains postfix `++`, `--` and `p#++` (points 1 to 3).
- A level for `?:` goes between the custom binary operators and assignment, and the compound assignments join the assignment level (points 6 and 9).
- `.*` joins level 7 (point 12), and the "Open" note under the table is reduced to the open questions below.
- The line `value = #pointer; // dereference` that #15 (item 8) found in level 4 is already gone from the page.

## Alternatives

- **A comma operator at the lowest level,** as in C and the table of the design chat, toward which emanspeaks first leaned. Not taken: point 14.
- **`++#p`** for the prefix pointer increment, the other spelling the design chat left open. Not taken: `#++p` (point 3).
- **A function or a keyword for the elementwise product,** which the first design session weighed against `.*`. Not taken: point 11.
- **Removing unary `..`,** which #15 offered beside documenting it. Not taken: it is reserved (point 13).

## Drawbacks

- `x++` inside a larger expression changes a variable in the middle of the expression, which needs an order of evaluation (open question 2).
- `^=` is exponentiation, which a C programmer reads as exclusive or.
- `.*` meets floating-point literals: `1.*x` is `1.` times `x` or `1 .* x`, depending on the form of the literals (#56, #57).
- The `:` of `?:` meets the root operator `::` and owned types `:#T` (#57): `c ? a : ::b` needs its spaces.
- Postfix `..` beside binary `..` has the shape of P-0012's `A~-B`: `a..-b` is `(a..) - b` or `a .. (-b)`.

## Open questions

1. **Assignment as an expression:** whether `=` and the compound assignments yield a value and group from the right (`a = b = c`, `if ((n = read()) > 0)`), or are statements only, as S0 has them (`s0.statement`).
2. **The order of evaluation** of what an expression changes, as in `a[i++] = i`: from left to right, unspecified, or an error where an expression reads a variable it also changes.
3. **The level of `as`:** the reference's table has none. Recommendation: a level of its own between the prefix operators and the binary root, as S0 has it provisionally (`s0.expr`), so that `-x as u8` converts `-x` and `a + b as u8` converts only `b`.
4. **Binary `..` and `..=`:** the reference lists them (`lex.op.binary`) at no level.
5. **Decorated compound assignment and `~`-decorated in-place forms:** whether `p #+= 2` and `w~++` exist, or a mutable borrow advances only as `w ~= w ~+ 1`; with the complete list of decorated operators (#26). `w~++` also meets the transpose (P-0012).
6. **A value context inside an address context:** the design chat's parentheses that open one, and `addrcontext()`, which the reference records as not reconciled with the rules (#15, item 6; #26).
7. **S0:** whether S0 gets unary `-`, `++` and `--`, compound assignment or `?:`. Recommendation: none until stage 1 needs one (`s0.purpose`); unary `-` maps onto core SIR's `neg` and a compound assignment onto a `set` or `store` of an operation if it does, while `?:` needs a rewrite into an `if`.

## Where it comes from

- #15 (moved from cesiumlang/cesiumlang.org#4, 2026-11-16): the items 1 to 10 of the reference page, and the design chat of 2025-09-03.
- 2026-11-22, on #15: bitwise NOT, `++` and `--` with `p#++` and `#++p`, `?:` just above assignment, the compound assignments, `.*`, unary `..` reserved, and unary `+`, `-` and `!` and the dereference line of level 4 as documentation fixes (points 1 to 3, 6, 9, 11 and 13).
- 2026-11-22, on #15: no comma operator (point 14), which superseded the earlier comment that he leaned toward a comma at the lowest level.
- 2026-11-22, on #26: `#==` and `#!=` (level 12).
- The reference page "Operators and Expressions": the 19 levels, and `#q[i]` (points 15 and 16).
