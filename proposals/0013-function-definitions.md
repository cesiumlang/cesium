+++
id = "P-0013"
type = "proposal"
status = "proposed"
+++

# Proposal P-0013: function definitions and assignments

This proposal decides where a function may be defined and how the parser tells a definition, `T = name(params) { ... }`, from an assignment of a call, `x = name(args);`. It carries the design issue #27, with the answer emanspeaks gave there on 2026-11-22 and the rules of the original design that the reference page "Functions" records. Where emanspeaks left a point to the proposal (2026-11-22), the point says it is a recommendation. emanspeaks accepts, changes or marks open each numbered point. Nothing in it is decided until he accepts it.

Constructors and the other function and method rules are #18's; generics are #29's; shadowing a name in scope is #75's.

## Motivation

Both forms start with a name, `=`, a name and `(`, and differ only in what follows the closing parenthesis and in whether the first name is a type (#27). Types the program defines are ordinary identifiers, so without a rule the parser needs a symbol table, the C "typedef problem", or unbounded lookahead. The problem already shows: the TextMate grammar's `meta.function.cesium` matches `result = func(a)` as a definition and guesses types by capitalization. With definitions allowed inside function bodies (point 1), both forms meet in the same block, so the rule has to hold everywhere.

## Design

### Where a function is defined

1. A function may be defined anywhere a declaration or a statement may be written: at namespace scope, in a struct, and inside a function body. Direction (option 2 of #27); the draft rule `s0.func.where`.
2. There are no prototypes or forward declarations, and declaration order does not matter. Original design. Outside an `extern` block, `T = name(params);` therefore never declares a function; inside one it declares a C function (`s0.extern`), and an `extern` block holds no assignments.
3. A definition has no introducer keyword such as `fn`. Original design (option 3 of #27 was rejected in September 2025).

### Telling a definition from an assignment

4. The token after the closing parenthesis decides: `{` starts a definition, `;` ends an assignment. Direction. Any other token there (`x = f(a) + 1;`, `x = f(a).len;`) continues the right side of an assignment. Recommendation: obvious, since a call is an expression that may go on.
5. To find that token, the parser scans ahead over the tokens of the statement, balancing parentheses, brackets and braces, to the first `=` outside them; if a name and `(` follow it, it scans on to the matching `)`. The scan never leaves the statement. Recommendation (left to the proposal): it is the lookahead the direction describes, bounded by one statement.
6. The parser keeps no symbol table: it decides by point 4 alone, without knowing which names are types. An assignment whose left side names a type, or a definition whose return type is not a type, is an error found by name resolution. Recommendation (left to the proposal): it avoids the typedef problem, and it lets a tool parse a file without its imports.
7. Multiple and owned returns are told apart the same way: `i32, f64 = divide(i32 a, i32 b) {` defines, `q, r = divide(a, b);` assigns, and `:#T, u8 = f() {` defines. Since there is no comma operator (`lex.op.no-comma`, direction in #15), a comma before `=` separates return types or assignment targets and nothing else. Recommendation (left to the proposal).
8. A statement that starts with a qualifier keyword (`static`, `private`, `secret`, `operator`, `property`, `context`, `destruct`) or with `generic` is never an assignment, so the parser needs no lookahead for it. Recommendation: obvious, since these are keywords that only declarations take (reference page "Functions").
9. The read-only method `T = #name() {` and the method-resolution alias `i32 = Left.some_method;` are written in a struct, where there are no assignments, so they are not ambiguous. Recommendation: obvious, since a struct body holds declarations only.

### S0

10. In S0 a function is defined only at the top level: `func_def` stays in `top_decl` (`s0.unit`), and `s0.func.where` holds for full Cesium. Recommendation (left to the proposal): stage 1 does not need nested functions, so by `s0.purpose` they stay out; stage 0 and `cesium1` already work so, and an S0 program stays a Cesium program (`s0.subset`).
11. With point 10, S0 has no assignment at the top level, and after a type the next token decides: `=` starts a definition, a name starts a global or a `const`. Stage 0 needs no lookahead beyond that. Consequence of point 10.

### The grammar and the tools

Once the points are accepted, these follow; they are corrections, not design:

- The TextMate and tree-sitter grammars (cesiumlang/grammar) match a definition by the `{` after the parameter list, and drop the capitalization heuristic.
- The reference page "Functions" says that a function may be defined inside another, and gives an example.

## Alternatives

- **Definitions only at namespace and struct scope** (option 1 of #27, the audit's suggestion), where expression statements are not allowed, so that one token of lookahead and the scope kind decide. Not taken by the direction; S0 keeps it (point 10).
- **An introducer keyword** such as `fn` (option 3). Rejected in September 2025 (point 3).
- **A symbol table in the parser,** as C compilers keep for `typedef` names. Not taken by point 6: the bounded scan of point 5 decides without one.

## Drawbacks

- The parser, and a reader, has to look past the whole parameter list to know what a statement is; a long parameter list on several lines puts the deciding `{` far from the start.
- Line-based tools, such as a TextMate grammar, cannot see a `{` on the next line, so they may still misclassify a definition whose brace is on its own line.
- A forgotten `;` before a block, if a block is ever a statement by itself, turns an assignment into a definition, and the error is reported on its parameters rather than at the missing `;`.

## Open questions

1. **The scope of a nested function:** whether it is visible in the whole enclosing block (as "declaration order does not matter" suggests) or only after its definition.
2. **What a nested function sees** of the enclosing function: its parameters and locals (a closure, which needs a representation in core SIR), or only globals and other functions.
3. **The method-resolution alias outside a struct** (point 9): whether `i32 = Left.some_method;` is written anywhere else, where it would meet an assignment of a field.

## Where it comes from

- #27 (the design audit of 2026-11-16): the two forms, the typedef problem, multiple and owned returns, the TextMate grammar's misclassification, and options 1 to 3.
- 2026-11-22, on #27 (recorded 2026-11-23): option 2, definitions anywhere, with the lookahead to `{` versus `;` (points 1 and 4); the lookahead, the symbol table, multiple and owned returns and S0 left to the proposal (points 5 to 7 and 10).
- 2026-11-22, on #15: no comma operator (point 7).
- The reference page "Functions", "Rules from the Original Design": no prototypes or forward declarations, declaration order does not matter, the qualifiers (points 2 and 8).
