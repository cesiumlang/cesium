+++
id = "P-0012"
type = "proposal"
status = "proposed"
+++

# Proposal P-0012: the tilde and bitwise NOT

This proposal decides how `~` next to an operator is read, as a postfix transpose followed by the operator or as a decorated operator, and how logical and bitwise NOT are written. It carries the design issue #28, with the answers emanspeaks gave there on 2026-11-22, which he gave on #15 too for bitwise NOT. Where the issue leaves a point to the proposal, the point says it is a recommendation. emanspeaks accepts, changes or marks open each numbered point. Nothing in it is decided until he accepts it.

The other operators of #15 and the precedence table are P-0014's; the complete list of decorated operators stays with #26.

## Motivation

`~` had five jobs: postfix transpose (`A~`), the mutable pointer type (`~T`), the mutable borrow (`~x`), the mutable-borrow assignment (`~=`), and the prefix of the `~`-decorated operators (`~+`; #11, rule 30). With the longest match (`lex.token.longest`), `A~*B` would be the decorated `~*`, so the most common product of linear algebra, a transpose times a matrix, would depend on a space. Bitwise NOT was prefix `~` in the first precedence table and lost its spelling when prefix `~` became the mutable borrow; the reference has none (#15, item 2). Each answer changes how the lexer cuts tokens, so it comes before the grammar (#57).

## Design

Each point names where it comes from: a direction of emanspeaks on #28 or #15 (2026-11-22), a decision of #11, or a recommendation.

### NOT

1. `!` is logical NOT and `!.` is bitwise NOT; `~` is not a NOT. Direction; the draft rule `lex.op.not`.
2. `!x` is 1 where `x` as a condition would be false and 0 otherwise, as a `u8`; it is core SIR's `lnot`. Recommendation: it is C's `!` with the truth values of #38 (`lex.predeclared.truth`, `s0.condition`).
3. `!.x` flips every bit of the integer `x`, in the type the expression has (`s0.expr.typing`); it is core SIR's `not`. Recommendation: it is the meaning of C's `~`.
4. `!` and `!.` are prefix operators, at the level of the other prefix operators (P-0014). Direction for `!` (#15, item 1: a documentation fix); recommendation for `!.`, which is a prefix operator like `!`.

### `~`

5. `~` keeps two uses only: the postfix matrix transpose, and the mutable pointer modifier, which is the type `~T`, the borrow `~x`, the assignment `~=` and the `~`-decorated operators such as `~+`. Direction.
6. `~` is a token of its own, never part of a longer operator: `~+`, `~*` and `~=` are each two tokens. It is an exception to the longest match. Direction; the draft rule `lex.token.tilde`.
7. Where `~` follows an operand and an operator follows the `~`, a pass after parsing, with types and context, decides whether the pair is a transpose followed by the operator or a decorated operator. Direction.
8. White space settles it where it can: `~` followed by white space is a transpose, so `A~ * B` and `A~ *B` are a transpose followed by `*`, and the lexer can say so. Direction.
9. `~` after white space and directly before the operator, as in `A ~* B`, is the decorated operator: the mirror of point 8. Recommendation; the issue leaves it to the proposal.
10. With no white space on either side (`A~*B`, `A~+B`, `A~=B`), the later pass reads the decorated operator where the operand is a pointer, since decorated operators are valid only in a pointer context (#11, rule 30), and the transpose where the operand is a value that can be transposed. Where both readings are possible, as for a `~T` pointer to a matrix, the program is in error; parentheses, `(A~)*B`, or white space say which. Recommendation: the issue presumes an error, which is what S0 does (point 12).
11. A decorated operator of either family is at the precedence level of the operator it decorates: `#+` and `~+` with `+`, `#==` with `==`. Both readings of `A~*B` then give the same tree, and the later pass changes only the operator and whether `A` is transposed. Recommendation: it is what lets the parser build the tree before the types are known.

### S0

12. Transpose and the decorated operators are not S0. Where `~` next to an operator could be either in S0 source, it is an error, and `(A~)*B` says which. Direction; the draft rule `s0.excluded.transpose`.

## Alternatives

- **Another spelling of transpose** (option 1 of #28): a postfix `'` is taken by character literals, and a method or a `$t$` operator is heavier. Not taken: transpose keeps `~`, its oldest use (point 5).
- **Another sigil for "mutable"** (option 2, one of the issue's two suggestions). Not taken: `~` keeps the mutable pointer modifier (point 5).
- **White space required before a decorated operator** (option 4, the other suggestion). Not taken: white space settles what it can (points 8 and 9), and the later pass reads `A~*B` (point 7).
- **A lexer that knows the context** (option 3), with type information in the lexer. Taken in part: the decision is made from types and context, but after parsing, and the lexer stays free of types (point 7).
- **Prefix `~` for bitwise NOT,** as in C and the first precedence table. Not taken: prefix `~` is the mutable borrow (point 1).

## Drawbacks

- The parser cannot settle `A~*B` alone: it keeps the pair undecided until the types are known, and every tool that reads Cesium without types (the highlighting grammar, the first pass of a language server) has to do the same or guess.
- Points 8 and 9 make white space significant around `~`, against `lex.token.space`. `~=` and the `~`-decorated operators must be written without a space inside them: `w ~ = v` is a transpose followed by `=`.
- A reader without the types has to guess at `A~*B` as the later pass does.

## Open questions

1. **`#` and the operator after it:** whether `#` is a token of its own, as `~` is (point 6), or `#+`, `#==` and `#++` stay single tokens by the longest match. Recommendation: single tokens; `#` has no postfix use, so `a#+b` can only be the decorated `+`.
2. **`!.` against `!` followed by `.`:** by the longest match, `!.5` is `!.` and `5`, and `!..r` is `!.` followed by `.`, where `!(.5)` or `!(..r)` may have been meant. Options: the longest match, with parentheses or a space for the other reading; or floating-point literals that start with a digit (#56), which leaves only unary `..`, reserved by P-0014. Recommendation: the longest match, as for every other token (`lex.token.longest`).
3. **`!.` in S0:** whether S0 has it. Recommendation: not until stage 1 needs it (`s0.purpose`); it maps one to one onto core SIR's `not` if it does.

## Where it comes from

- #28 (filed 2026-11-16 from the design audit): the collisions and the options 1 to 4.
- 2026-11-22, on #28: bitwise NOT `!.`, the two uses of `~`, `~` as a token of its own with the later pass, white space, and S0 (points 1, 5 to 8 and 12); what the later pass does when it cannot tell, `A ~* B`, and the tokens of `!.` left to the proposal.
- 2026-11-22, on #15: bitwise NOT `!.` (point 1), and unary `!` into the precedence table as a documentation fix (point 4).
- #11, rule 30: the decorated operators, valid only in a pointer context (point 10).
