# Diagnostics

The errors `cesium0` and `cesium1` report in S0 source, by code (cesiumlang/cesium#82). `cesium0` prints `file:line:col: error[E0101]: message` on standard error; `cesium1` prints the same on standard output, or `line:col: error[E0101]: message` for source given on standard input, with lines counted from the start of the input. Both give the same code and the same message for the same error.

Each code is stable once released: what it means does not change, and a code is never reused for another message. A message is shown as a template, in which the parts in angle brackets (`<name>`, `<type>`, `<token>`, `<n>`) are filled in by the compiler. The first two digits of a code are its group; the last two count within the group, first in the order of first appearance in the source of `cesium0`, and a code added later takes the next free number of its group. `zig build test` checks that every code here is used, and that every code the compilers use is here and is used by both (`build-src/diagnostics.zig`).

Not covered yet, and printed without a code: the messages of `libcsmir`, its MIR text parser and its verifier (`cesium0: error: ...`, and in `cesium1`, for the two verifier checks it makes itself, `line:col: error: ...`); the I/O messages of both compilers (`cesium1`: `error: cannot read a.csm`) and the usage errors of `cesium1` (exit status 2); and the limits of `cesium1` itself (`error: internal: ...`, exit status 2).

## Lexical (E01xx)

### E0101

`unterminated comment`

A `/*` comment has no closing `*/` before the end of the file. Close it; the first `*/` ends the comment.

### E0102

`'<name>' is a Cesium keyword that S0 does not have`

S0 source must stay valid Cesium, so the keywords of full Cesium that S0 does not use, such as `type`, `list`, `error`, `null` and `for`, cannot be names either. Choose another name; for a loop, S0 has `while`.

### E0103

`S0 has decimal integer literals only`

A number is followed by a letter: hexadecimal, binary and octal forms (`0x1F`) and suffixes (`10u`) are not in S0 (cesiumlang/cesium#56). Write the value in decimal, and give it a type with `as T` where the context gives none.

### E0104

`integer literal is too large`

The literal is larger than 18446744073709551615, the largest `u64`, so no S0 type holds it.

### E0105

`unterminated literal`

A string or character literal has no closing quote before the end of its line. A literal cannot span lines; write `\n` for a line break.

### E0106

`unknown escape: stage 0 accepts \n \t \r \0 \\ \' \"`

A backslash in a literal is followed by a character that is not one of the escapes listed, as in `\x41`. The escapes of Cesium are not decided yet (cesiumlang/cesium#39); use a listed one, or the character's value as a number.

### E0107

`unexpected character`

The character begins no token of S0, such as `^`, `@`, `?` or `$`. Exclusive or is written `><`, and S0 has no `?:`; write an `if`.

## Syntax (E02xx)

### E0201

`expected <token>, found <token>`

The parser needed a particular token, such as `;`, `)` or `=`, and found another. Where the token found is a habit from another language, the message says so: E0214 for a C-style function header, E0215 for a return type after `->`.

### E0202

`expected a name, found <token>`

A declaration, a field access or a parameter needs a name here, and a keyword, a literal or a symbol stands in its place.

### E0203

`':' marks an owned parameter, return value or field; it cannot be used here`

The ownership mark `:` (as in `:#T`) belongs to the types of parameters, return values and struct fields (cesiumlang/cesium#11). Remove it from the types of locals, constants, `as` and `sizeof`.

### E0204

`':' must be followed by a pointer type`

Ownership applies to pointers only: write `:#T` or `:~T`, not `:T`.

### E0205

`'void' is only a return type in S0`

`void` can be the return type of a function, as in `void = f()`, but not the type of a variable, a parameter or a field, nor the target of a pointer.

### E0206

`expected a type, found <token>`

A type is needed here: a number type (`u8`, `i32`, `u32`, `i64`, `u64`, `uword`) or the name of a struct, after any `#` or `~`.

### E0207

`S0 has no unary minus; write 0 - x`

Unary minus is not among the operators of S0 and has no place in the precedence table yet (cesiumlang/cesium#15). Subtract from zero instead: `0 - x`.

### E0208

`expected an expression, found <token>`

An operand is missing, as in `a + ;` or `f(1, )`, or a statement begins with a token that begins no statement. Prefix `&` and `*`, compound assignment and `++` and `--` have messages of their own: E0216 to E0219.

### E0209

`only a function name can be called in S0`

S0 has no function values, so a call through an expression, such as `s.f()` or `(f)()`, cannot be written. Call the function by its name.

### E0210

`an array length is an integer literal in S0`

The length between `[` and `]` in an array's declaration is a decimal literal, as in `u8 buf[64];`, not a constant or an expression.

### E0211

`a struct field has no initializer`

A field is declared as `type name;`: S0 has no default field values. Set the field after declaring a variable of the struct.

### E0212

`a block is not a statement by itself in S0`

A `{ ... }` block is only the body of a function, an `if`, an `else` or a `while`. Remove the braces, or write the `if` or `while` that was meant.

### E0213

`expected a library name such as 'c'`

The `import` of an `extern` block names its library in quotes, as in `extern libc = import('c') { ... }`.

### E0214

`expected ';', found '('; a function is written 'T = name(parameters) { ... }'`

A declaration at the top level, such as `int main() {` or `fn main() {`, continues with a parameter list, so it reads as a global named `main` followed by `(`. An S0 function is declared with its return type, `=`, its name and its parameters: `i32 = main(i32 argc, ##u8 argv) { ... }` (rule `s0.func`).

### E0215

`expected '{', found '->'; the return type comes first: 'T = name(parameters)'`

A function header continues with `->` and a return type after its parameters, as in Rust or Zig. In S0 the return type comes before the name, followed by `=`: `i32 = main() { ... }`, and `void = f()` for a function that returns nothing (rule `s0.func`).

### E0216

`expected an expression, found '&'; an address is taken with '#'`

An expression begins with `&`, C's address-of. In S0 `&` is the bitwise and of two operands only; an address is taken with `#`, as in `f(#x)` for a pointer parameter (rule `s0.expr`).

### E0217

`expected an expression, found '*'; there is no dereference operator: p, p[i] and p.f read through a pointer`

An expression begins with `*`, C's dereference. S0 has no dereference operator: a pointer is read through by default, so `p` where a number is expected is the value it points at, and `p[i]` and `p.f` reach through it (rule `s0.expr.deref`). Write `return p;` for C's `return *p;`, and `p = 1;` for `*p = 1;`, since plain `=` assigns through a pointer.

### E0218

`compound assignment is not decided for S0; write x = x + y`

An operator such as `+`, `-`, `<<` or `><` is followed by `=`, as in `x += 1`. The compound assignments are operators of full Cesium by the direction of cesiumlang/cesium#15 (rule `lex.op.compound`), but whether S0 has them is open, so stage 0 rejects them. Write the assignment out: `x = x + 1`.

### E0219

`'++' and '--' are not decided for S0; write x = x + 1`

Two `+` or two `-` with nothing between them stand before or after an operand, as in `i++` or `--i`. Increment and decrement are operators of full Cesium by the direction of cesiumlang/cesium#15 (rule `lex.op.increment`), but whether S0 has them is open, so stage 0 rejects them. Write `i = i + 1` or `i = i - 1`.

## Names and declarations (E03xx)

### E0301

`unknown type '<name>'`

The name is neither a number type of S0 nor a struct of the unit. The type names of C have messages of their own: E0320 to E0322.

### E0302

`'<name>' is already the name of <what>`

Whether Cesium allows shadowing is not decided, so S0 allows none: a name is taken by a local or parameter in scope, a global, a constant, a function, a struct or the built-in `alloc`, and cannot be declared again. Rename one of them.

### E0303

`'<name>' is a function`

A function's name stands where a variable is needed: as a value, on the left of an assignment, or under `#`. S0 has no function values; call it as `name(...)`.

### E0304

`unknown name '<name>'`

No local, parameter, global or constant of this name is in scope here. Check the spelling, and that a local is declared before its use. For `true` and `false`, the message is E0323.

### E0305

`unknown function '<name>'`

No function of this name is declared in the unit. A function of the C library is declared in an `extern` block, as in `extern libc = import('c') { i32 = putchar(i32 c); }`.

### E0306

`an array has at least one element`

An array is declared with length 0. S0 has no empty arrays, and nothing like C's zero-length trailing arrays.

### E0307

`struct '<name>' is declared twice`

Two structs of the unit have the same name, perhaps in different files compiled together. Rename or remove one.

### E0308

`struct '<name>' has no fields`

A struct declares at least one field.

### E0309

`struct '<name>' has two fields named '<field>'`

The names of the fields of a struct are distinct. Rename one.

### E0310

`a constant is a number in S0, not a <type>`

A `const` has a number type, as in `const u32 SIZE = 64;`. Pointers, strings, arrays and structs are not constants; declare a global instead.

### E0311

`'<name>' is a <type>, not a <type>`

A constant is initialized with another constant of a different type, and S0 converts no constant. Give both the same type.

### E0312

`a constant is initialized with a literal or another constant in S0`

The value of a constant is an integer literal or a constant declared before it. S0 does not evaluate expressions such as `A + 1` in declarations.

### E0313

`a global is initialized with a literal or a constant of its type in S0`

A number global is initialized with an integer literal that fits its type, or with a constant of the same type. Set it to anything else in code, for example at the start of `main`.

### E0314

`a global pointer is initialized with a string in S0, or not at all`

A global `#u8` may point at a string literal; any other global pointer is declared without an initializer and set in code.

### E0315

`a <type> has no initializer in S0`

A global array or struct cannot be initialized: what `=` does to an aggregate is not decided (cesiumlang/cesium#22). Declare it without an initializer, which leaves it zero, and set its elements or fields in code.

### E0316

`'<name>' returns a <type>; S0 functions return a number, a pointer or nothing`

A function cannot return an array or a struct. Return a pointer, or take a pointer to where the result goes, as in `void = make(~Point out)`.

### E0317

`parameter '<name>' is a <type>; S0 parameters are numbers and pointers`

Arrays and structs are not passed by value. Take a pointer to them, `#T` or `~T`, instead.

### E0318

`two parameters are named '<name>'`

The names of the parameters of a function are distinct. Rename one.

### E0319

`'cs_arena_alloc' is the runtime function behind alloc and cannot be declared`

The unit uses `alloc`, which calls the runtime function `cs_arena_alloc`, and also declares a function of that name. Rename that function.

### E0320

`unknown type '<name>'; the integer types of S0 are u8, i32, u32, i64, u64 and uword`

The type is one of C's integer types, `int`, `long`, `short`, `char`, `unsigned`, `signed` or `size_t`, and no struct of the unit has its name. S0 names its integer types by sign and width (rule `s0.type.integers`): write `i32` for `int`, `u8` for `char`, `uword` for `size_t`, and `#T` for `T *`.

### E0321

`unknown type 'bool'; there is no bool type: a condition is an integer, nonzero is true`

Cesium has no `bool` type, by the direction of cesiumlang/cesium#38 (rule `lex.predeclared.truth`): the condition of an `if` or a `while` is an integer, and nonzero is true (rule `s0.condition`). Declare a flag as a `u8`; a comparison such as `a < b` gives 1 or 0.

### E0322

`unknown type '<name>'; S0 has no floating point`

The type is `float` or `double`, and floating point is not in S0 (rule `s0.excluded`). Stage 0 and stage 1 compute with integers only.

### E0323

`unknown name '<name>'; true and false are not decided for S0: write 1 or 0`

The name is `true` or `false`, and no constant of the unit has it. Full Cesium is to have them as built-in `u8` constants with the values 1 and 0, by the direction of cesiumlang/cesium#38 (rule `lex.predeclared.truth`), but whether S0 has them is open, so stage 0 does not. Write `1` or `0`, or declare constants such as `const u8 TRUE = 1;`.

## Types and conversions (E04xx)

### E0401

`a <type> is not a value; use one of its <parts>`

An array or a struct, reached through an index, a field or a pointer, is used as a value: S0 copies, compares and passes only numbers and pointers. Use its elements (`a[i]`) or fields (`s.f`), or take its address with `#`.

### E0402

`expected <type>, found <type>`

The value has another type than its context declares, such as a pointer to one type where a pointer to another is expected. A plain value passed for a pointer parameter needs its address taken, `#x` or `~x`; S0 has no implicit conversions.

### E0403

`expected <type>, found <type>; convert it with 'as'`

Both are number types, but different ones, and S0 converts no number implicitly, unlike C with its integer promotions. Convert it, as in `n as u64`.

### E0404

`expected a pointer, found <type>`

A pointer is needed here, such as the operand of `as` with a pointer type, and the value is not one. Where a `uword` holding an address is accepted too, the message reads `expected a pointer or a uword, found <type>`.

### E0405

`expected a number, found <type>`

A number is needed here, such as the condition of an `if` or `while` or an operand of an operator, and the value has another type. A condition in S0 is a number, false when zero.

### E0406

`the type of these literals is not determined by their context; write 'as T' after one`

All the operands of an operator are literals, as in `1 < 2`, and nothing else gives them a type. The default type of an integer literal is not decided (cesiumlang/cesium#9, open question 2): write `1 as u32 < 2`.

### E0407

`the operands are <type> and <type>; convert one with 'as'`

The two operands of an arithmetic or comparison operator have different types, and S0 has no implicit conversions. Convert one, as in `(a as u64) + b`.

### E0408

`an address converts to uword only, not to <type>`

An address, `#x`, `~x` or a string, converted to a number with `as` becomes a `uword`. Convert that further if another type is needed: `(#x as uword) as u64`.

### E0409

`'as' converts to a number or a pointer type, not to <type>`

The target type of `as` is an array or a struct. Only numbers and pointers convert.

### E0410

`expected <type>, found a number`

An integer literal stands where a pointer is expected. S0 has no `null` (cesiumlang/cesium#26); pass the address of a variable, `#x`, or a pointer.

### E0411

`the type of this literal is not determined by its context; write 'as T' after it`

An integer literal stands where nothing gives it a type, as in `if (1)` or `!0`. The default type of an integer literal is not decided (cesiumlang/cesium#9, open question 2): write `1 as u8`.

### E0412

`<n> does not fit <type>`

The literal is outside the range of the type its context gives, as `300` for a `u8`. Use a wider type, and convert with `as` where needed.

### E0413

`a string is a #u8, not a number`

A string literal stands where a number is needed. A string is a pointer to its bytes; for one character, write a character literal such as `'a'`.

### E0414

`'<name>' is a <type>, not a value; use one of its <parts>`

A variable that is an array or a struct is used as a value: S0 copies, compares and passes only numbers and pointers. Use its elements or fields, or pass its address, `#name`.

### E0415

`an address is not a number; a pointer is expected only where one is declared`

`#x` or `~x` stands where a number is expected. An address is taken for a pointer parameter, the right side of `:=`, `#=` or `~=`, or a cast to a pointer; a pointer's value as a number is `#x as uword`.

## Places, assignment and initialization (E05xx)

### E0501

`'<name>' is a constant: it has no address and cannot be assigned`

A constant stands where a variable is needed: on the left of an assignment, under `#` or `~`, or indexed. Declare a global if it must change or have an address.

### E0502

`a <type> cannot be indexed`

Only an array or a pointer is indexed with `[i]`.

### E0503

`a pointer to void cannot be indexed`

The elements of a pointer to `void` have no size. Declare the pointer with its element type.

### E0504

`a <type> has no fields`

`.name` is applied to a value that is not a struct or a pointer to one. `.` reaches through pointers, so C's `p->f` is `p.f` in S0.

### E0505

`struct <name> has no field '<field>'`

The struct declares no field of this name. Check the spelling against its declaration.

### E0506

`this expression is not a variable, an element or a field`

The base of `[i]` or `.name` is a call or another expression that has no place in memory. Store it in a local first.

### E0507

`alloc(count) can only initialize or be assigned to a pointer with ':='`

`alloc` is not an ordinary function: it appears only on the right of `:=`, which gives it the element type of the pointer on the left, as in `~u8 buf := alloc(64);`.

### E0508

`'#' and '~' take the address of a variable, an element or a field`

The operand of `#` or `~` is a literal, a call or another expression without an address. Store it in a local and take the local's address.

### E0509

`alloc takes one argument, the element count`

`alloc(n)` allocates `n` elements of the pointer's element type; the size in bytes follows from the type, unlike C's `malloc(n * sizeof(T))`.

### E0510

`alloc needs an element type; the pointer is #void`

`alloc` sizes its block from the element type of the pointer, and `void` has no size. Declare the pointer with an element type.

### E0511

`alloc returns ownership; receive it with ':='`

The block `alloc` returns is owned, and ownership is received with `:=`, not borrowed with `#=` or `~=` (cesiumlang/cesium#11).

### E0512

`a <type> has no initializer in S0; assign its <parts> one at a time`

A local array or struct cannot be initialized: what `=` does to an aggregate is not decided (cesiumlang/cesium#22). Declare it without an initializer, which leaves it zero, and assign its elements or fields.

### E0513

`a pointer is initialized with ':=', '#=' or '~='; plain '=' assigns through it`

Plain `=` stores at the place a pointer points to and never sets the pointer itself. Set a pointer with `:=`, which takes ownership, or `#=` or `~=`, which borrow.

### E0514

`<operator> is only for pointers; a plain value is initialized with '='`

`:=`, `#=` and `~=` set where a pointer points. A number is initialized and assigned with `=`.

### E0515

`the left side of an assignment is a variable, an element or a field`

The left side is a call, a literal or another expression. Assign to a variable, an element `a[i]` or a field `s.f`.

### E0516

`a <type> is not assigned as a whole in S0; assign its <parts>`

An array or a struct is assigned with `=`, and what `=` does to an aggregate is not decided (cesiumlang/cesium#22). Copy it one element or field at a time.

### E0517

`<operator> sets where a pointer points; the left side is a <type>`

`:=`, `#=` and `~=` take a pointer on their left. A number is assigned with `=`.

## Calls and statements (E06xx)

### E0601

`'<name>' takes <n> arguments, not <n>`

The call passes another number of arguments than the function has parameters. S0 has no default arguments and no variadic functions.

### E0602

`'<name>' returns nothing`

A function declared `void = name(...)` is called where its value is used, or its body has `return value;`. Call it as a statement, or declare the type it returns.

### E0603

`only a call can be a statement`

An expression statement is a call; an expression such as `x;` or `a == b;` alone does nothing. Assign its value, or write the call that was meant.

### E0604

`'<keyword>' outside a loop`

`break` or `continue` appears outside the body of a `while`.

### E0605

`'<name>' returns a <type>`

`return;` without a value appears in a function that returns a value. Return a value of its type.

## Limits of S0 (E09xx)

### E0901

`nested too deeply (the limit is 128 levels)`

Expressions and statements nest at most 128 levels deep, so that no compiler overflows its stack: an operand, the body of an `if` or `while`, each further `else if` and each further operator of a chain such as `a + b + c` is a level. Split the expression with locals, or the statements into functions.

### E0902

`too many arguments`

A call passes more than 64 arguments. Pass a pointer to a struct instead.

### E0903

`too many parameters`

A function declares more than 64 parameters. Take a pointer to a struct instead.
