# Lexical structure

How source text is divided into tokens: source files, white space, comments, identifiers, keywords, literals, operators and punctuation. See [the introduction](index.md) for the categories, the statuses and the sources rules give.

## Source files

%rule[lex.source.extensions syntax draft introduced-by=P-0000] A Cesium source file has the extension `.csm`, and a module interface file the extension `.csi`. No other extension is used. Decided in [#14].

%rule[lex.source.ascii syntax draft introduced-by=P-0000] The syntax of the language uses ASCII characters only: every keyword, identifier, operator and punctuation mark is ASCII. Decided in the reference ([overview], "Design Goals").

Text is UTF-8 ([#39], direction of 2026-11-24), so comments and string literals can hold any character; Zig's WTF-8 may be adopted later, with the same behavior for valid text.

Open: the encoding of a source file as a whole, and whether a byte order mark is allowed ([#39]).

## White space and tokens

%rule[lex.token.space syntax draft introduced-by=P-0000] Space, horizontal tab, carriage return and line feed separate tokens and are otherwise not significant, except where %[lex.token.tilde] uses them. Provisional ([#57]): every example in the reference is consistent with it.

%rule[lex.token.longest syntax draft introduced-by=P-0000] At each point the lexer takes the longest sequence of characters that forms a token, so `#=` is one token and not `#` followed by `=`. Provisional ([#57]).

%rule[lex.token.tilde syntax draft introduced-by=P-0000] `~` is a token of its own, never part of a longer operator: `~+` is the two tokens `~` and `+`. Whether such a pair is a postfix transpose followed by an operator or a decorated operator is decided after lexing, from types and context. White space settles it where it can: `A~ * B` and `A~ *B` are a transpose followed by `*`. Direction in [#28], 2026-11-22.

Open: what the later pass does when it cannot tell `A~*B` apart (presumably an error); whether `A ~* B` is the other case white space settles; whether `#` and the operator after it are lexed the same way ([#28]). The other collisions of the longest match, such as `::` (root) against `:` followed by an owned type `:#T`, are listed in [#57].

## Comments

%rule[lex.comment.line syntax draft introduced-by=P-0000] A line comment starts with `//` and ends at the end of the line. Decided in the reference ([lexical-structure]).

```cesium %example[.alone accepts]
// a unit with nothing but a comment
```

%rule[lex.comment.block syntax draft introduced-by=P-0000] A block comment starts with `/*` and ends with the next `*/`. A block comment that is not closed is an error. Decided in the reference ([lexical-structure]).

```cesium %example[.open rejects error=E0101]
/* not closed
```

%rule[lex.comment.doc syntax draft introduced-by=P-0000] A comment that starts with `/**` is a documentation comment, for the declaration that follows it. It ends as a block comment does. Decided in the reference ([lexical-structure]).

```cesium %example[.before accepts]
/** The number of bytes of the text at s, before its NUL. */
uword = cstr_len(#u8 s) {
  uword n = 0;
  while (s[n] != 0) {
    n = n + 1;
  }
  return n;
}
```

Open: whether block comments nest (stage 0 ends one at the first `*/`); the tags recognized inside documentation comments, such as `@param` and `@return` (cesiumlang/grammar#4).

## Identifiers

%rule[lex.ident syntax draft introduced-by=P-0000] An identifier starts with a letter or an underscore and continues with letters, digits and underscores. Decided in the reference ([lexical-structure]):

```text
identifier = ( letter | "_" ) { letter | digit | "_" } ;
letter     = "A" ... "Z" | "a" ... "z" ;
digit      = "0" ... "9" ;
```

%rule[lex.ident.keyword legality draft introduced-by=P-0000] A keyword (%[lex.keywords]) is not an identifier. Decided in the reference ([lexical-structure]).

```cesium %example[.list rejects error=E0102]
i32 = main(i32 argc, ##u8 argv) {
  i32 list = 0;
  return list;
}
```

Open: a limit on the length of an identifier; whether identifiers that start with an underscore are reserved.

## Keywords

%rule[lex.keywords syntax draft introduced-by=P-0000] These words are reserved. Decided in the reference ([lexical-structure]), with `undefined` added by [#11], rule 15:

- %rule[.types syntax draft introduced-by=P-0000] Core types: `file` `str` `list` `dict` `slice` `struct` `path` `uword`. The built-in types stay keywords, not names in a namespace: direction in [#50], 2026-11-22.
- %rule[.constructs syntax draft introduced-by=P-0000] Language constructs: `alias` `catch` `as` `return` `comptime` `generic` `sizeof` `typeof` `asm` `void` `enum` `error` `null` `undefined` `union`
- %rule[.control syntax draft introduced-by=P-0000] Control flow: `if` `while` `for` `else` `do` `with` `defer` `break` `continue` `switch` `case` `fallthrough` `throw`
- %rule[.function syntax draft introduced-by=P-0000] Function qualifiers: `operator` `context` `property` `private` `secret` `static` `destruct`
- %rule[.variable syntax draft introduced-by=P-0000] Variable qualifiers: `const` `static` `private` `secret` `volatile` `atomic` `register` `simd`
- %rule[.interop syntax draft introduced-by=P-0000] C interop: `extern` `export`
- %rule[.objects syntax draft introduced-by=P-0000] Object-oriented constructs: `trait` `impl` `type` `this` `super`
- %rule[.namespaces syntax draft introduced-by=P-0000] Namespacing: `std` `namespace`

Open ([#50], [#20]): which of `type`, `error`, `context`, `property`, `register`, `secret`, `static`, `this`, `super`, `union` and `enum` stay reserved; `import`, used as a keyword by modules and by S0, is not in the list; whether the sized type names (`u8`, `i32`, `f64`, and widths such as `u34`) are keywords or predeclared names.

### Predeclared names

%rule[lex.predeclared.shadowable static draft introduced-by=P-0000] The mathematical built-ins (`min`, `max`, `abs`, `mean`, `norm`, `round` and the like) are not keywords. They are predeclared names, which a program may shadow. Direction in [#50], 2026-11-22.

%rule[lex.predeclared.fixed legality draft introduced-by=P-0000] `std` and the memory built-ins `alloc` and `free` are never shadowed, even where they are not keywords: a declaration of one of these names is an error. Direction in [#50], 2026-11-22.

%rule[lex.predeclared.truth static draft introduced-by=P-0000] `true` and `false` are predeclared constants of type `u8`, with the values 1 and 0. There is no `bool` type. Direction in [#38], 2026-11-22.

Open: the full list of memory built-ins that are never shadowed ([#50]); whether `stdin`, `stdout` and `stderr` are reserved ([#50]); shadowing in general ([#75]).

## Literals

### Integer literals

%rule[lex.lit.int.suffix syntax draft introduced-by=P-0000] An integer literal has no type suffix. Decided in [#16].

%rule[lex.lit.int.decimal syntax draft introduced-by=P-0000] A decimal integer literal is a sequence of decimal digits. By use, in the reference and the S0 design:

```text
decimal_literal = digit { digit } ;
```

%rule[lex.lit.int.type static draft introduced-by=P-0000] An integer literal takes its type from its context: the type of the destination it is written to, or of the operand beside it. With no context, it is an `i32` if its value fits one, and an `i64` otherwise. Decided in [#16] (from context); direction in [#33], 2026-11-22 (without context).

Open ([#56], [#16]): hexadecimal (`0xFF`), binary (`0b1010`) and octal (`0o755`) literals, which the reference uses in examples and stage 0 rejects; digit separators; a literal that does not fit its type.

### Floating-point literals

%rule[lex.lit.float.type static draft introduced-by=P-0000] A floating-point literal with no context is an `f64`. Direction in [#33], 2026-11-22.

Open: the form of a floating-point literal ([#56]). The reference writes `3.14159`; exponents and every other detail are unspecified, and the form meets the range operator in `1..2` ([#57]).

### Character literals

%rule[lex.lit.char syntax draft introduced-by=P-0000] A character literal is one ASCII character, or an escape sequence, between single quotes: `'A'`. Its value is that character's code, one byte; there is no character type. By use ([type-system], [#9]); one byte, direction in [#39], 2026-11-24.

```cesium %example[.letter accepts]
u8 letter = 'A';
u8 newline = '\n';
```

%rule[lex.lit.char.ascii legality draft introduced-by=P-0000] A character literal that holds a character outside ASCII, such as `'é'`, is an error; such text is written as a string, `"é"`. Direction in [#39], 2026-11-24, which replaced an earlier answer that took such a literal as its UTF-8 bytes.

```cesium %example[.accented rejects error=E0108]
u8 c = 'é';
```

Open ([#39]): the set of escape sequences (stage 0 accepts `\n`, `\t`, `\r`, `\0`, `\\`, `\'` and `\"`); whether a byte from 128 to 255 can be written as a character literal, by an escape for example.

### String literals

%rule[lex.lit.string syntax draft introduced-by=P-0000] A string literal is text between double quotes, with escape sequences. Decided in the reference ([lexical-structure]).

%rule[lex.lit.string.interpolated syntax draft introduced-by=P-0000] An interpolated string literal is text between backquotes, with expressions embedded between `{` and `}`. Decided in the reference ([lexical-structure]).

%rule[lex.lit.string.type static draft introduced-by=P-0000] A string literal is a static `str`: a pointer and a length, in bytes. It is stored with a NUL after its last byte, so it also converts implicitly to `#u8`. Direction in [#39], 2026-11-24.

Open ([#39]): the escape sequences; the grammar of the expressions in an interpolated string; what a `str` owns; passing a `str` to a C `char*`; wide strings.

### Other literals

%rule[lex.lit.null static draft introduced-by=P-0000] `null` and `undefined` are keywords and are different from each other. `null` is assigned deliberately. `undefined` is the state of a pointer that owns nothing and of a plain value that is not initialized, and nothing tests for it. Decided in [#11], rules 15, 16 and 19.

## Operators and punctuation

%rule[lex.op.assign syntax draft introduced-by=P-0000] The assignment operators are `=`, `#=`, `~=` and `:=`. The last three are address-context operators: they set where a pointer points. Decided in [#11], rule 2.

%rule[lex.op.compound syntax draft introduced-by=P-0000] The compound assignment operators are `+=`, `-=`, `*=`, `/=`, `%=`, `^=`, `&=`, `|=`, `<<=`, `>>=`, `**=` and `><=`, at the level of assignment. Direction in [#15], 2026-11-22.

%rule[lex.op.sigils syntax draft introduced-by=P-0000] `#` and `~` are type sigils (`#T`, `~T`) and prefix operators: address-of for an immutable and for a mutable borrow. `:` in front of a sigil marks an owned type: `:#T`, `:~T`. Decided in [#11], rules 1, 20 and 22.

%rule[lex.op.decorated syntax draft introduced-by=P-0000] Address arithmetic is written with decorated operators, in two families, `#`-decorated and `~`-decorated: `#+`, `~+`, and in-place forms such as `#++`. Decided in [#11], rule 30.

%rule[lex.op.address-compare syntax draft introduced-by=P-0000] The decorated comparison operators `#==` and `#!=` compare addresses; a pointer is tested against null with `p #== null`. Direction in [#26], 2026-11-22.

%rule[lex.op.binary syntax draft introduced-by=P-0000] The binary and unary operators of the reference's precedence table are `::`, `^`, `*`, `/`, `%`, `@`, `**`, `+`, `-`, `<<`, `>>`, `<`, `>`, `<=`, `>=`, `==`, `!=`, `&`, `><` (exclusive or), `|`, `&&`, `||`, postfix `~` (transpose), the range operators `..` and `..=`, and custom operators of the form `$name$`. Decided in the reference ([operators-expressions]).

%rule[lex.op.elementwise syntax draft introduced-by=P-0000] `.*` is the elementwise product; `*` stays the product that depends on its operands. Direction in [#15], 2026-11-22.

%rule[lex.op.not syntax draft introduced-by=P-0000] `!` is logical NOT and `!.` is bitwise NOT. `~` is not a NOT. Direction in [#15] and [#28], 2026-11-22.

%rule[lex.op.increment syntax draft introduced-by=P-0000] `++` and `--` are both prefix and postfix operators, as in C. On a pointer, the decorated forms move the address: `p#++` after, `#++p` before. Direction in [#15], 2026-11-22.

%rule[lex.op.conditional syntax draft introduced-by=P-0000] The conditional operator is `a ? b : c`, at one level just above assignment. Direction in [#15], 2026-11-22.

%rule[lex.op.no-comma syntax draft introduced-by=P-0000] There is no comma operator. A comma only separates arguments, items, initializers, and the targets and types of multiple returns; it has no precedence level. Direction in [#15], 2026-11-22.

%rule[lex.op.unary-range syntax draft introduced-by=P-0000] Unary `..` is reserved: it keeps a place in the precedence table, and what it does is left to a later proposal. Direction in [#15], 2026-11-22.

%rule[lex.punctuation syntax draft introduced-by=P-0000] The punctuation is `(` `)` `[` `]` `{` `}` `,` `;` `.`, and `...` for variadic parameters. By use ([functions]).

Open: how `!.` is told apart from `!` followed by a number such as `.5` or by unary `..` ([#28]); the complete list of decorated operators, and the spelling of the level count that reaches the middle of a `##T` ([#26]); unary `+` and `-` in the precedence table ([#15]); the angle brackets of `generic<T>` ([#29]).

[#11]: https://github.com/cesiumlang/cesium/issues/11
[#14]: https://github.com/cesiumlang/cesium/issues/14
[#15]: https://github.com/cesiumlang/cesium/issues/15
[#16]: https://github.com/cesiumlang/cesium/issues/16
[#20]: https://github.com/cesiumlang/cesium/issues/20
[#26]: https://github.com/cesiumlang/cesium/issues/26
[#28]: https://github.com/cesiumlang/cesium/issues/28
[#29]: https://github.com/cesiumlang/cesium/issues/29
[#33]: https://github.com/cesiumlang/cesium/issues/33
[#38]: https://github.com/cesiumlang/cesium/issues/38
[#39]: https://github.com/cesiumlang/cesium/issues/39
[#50]: https://github.com/cesiumlang/cesium/issues/50
[#56]: https://github.com/cesiumlang/cesium/issues/56
[#57]: https://github.com/cesiumlang/cesium/issues/57
[#75]: https://github.com/cesiumlang/cesium/issues/75
[#9]: https://github.com/cesiumlang/cesium/issues/9
[overview]: https://cesiumlang.org/reference/overview/
[lexical-structure]: https://cesiumlang.org/reference/lexical-structure/
[type-system]: https://cesiumlang.org/reference/type-system/
[operators-expressions]: https://cesiumlang.org/reference/operators-expressions/
[functions]: https://cesiumlang.org/reference/functions/
