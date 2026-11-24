# The Cesium Language Specification

This is the normative definition of Cesium: what a compiler or a tool is checked against. The [language reference](https://cesiumlang.org/reference/) explains Cesium by example. This specification is the technical document behind it, and where the two disagree, this one is meant to win once its rules are final.

Nothing here is final yet. Every rule starts as `draft` and becomes `final` only through an accepted proposal (%[P-0000]).

## How it is written

The specification is Markdown, one chapter per file, read by [Skein](https://github.com/chasingdaylight/skein). A **rule** is a paragraph or a list entry that starts with a marker. The marker gives the rule's ID, its category and its status, and links it to the proposal that introduced it:

```markdown
%rule[lex.comment.line syntax draft introduced-by=P-0000] A line comment starts with `//` and ends at the end of the line.
```

- **The ID** says what the rule is about, and never changes. A rule that is dropped keeps its ID, with the status `removed`.
- **The category** is one of `syntax` (the form of the source), `legality` (what the compiler must reject), `static` (meaning fixed at compile time) and `dynamic` (behavior at run time), as in the Ferrocene Language Specification.
- **The status** is `draft` (written down, not accepted), `final` (accepted through a proposal), `deprecated` or `removed`.
- **Parts:** a list entry with its own marker inside a rule is a part of that rule, with the rule's ID and its own name: `lex.keywords.types`.
- **References:** a rule names another rule as `%[lex.ident]`.
- **Examples:** a code block whose opening line carries `%example` is a tested example of the rule it is in. Its category, `accepts` or `rejects`, says whether a compiler must accept it.

Text without a marker is **narrative**: it explains and decides nothing.

### Where a draft rule comes from

Each draft rule ends with its source, so a reader can tell how firm it is:

| Source | Meaning |
| --- | --- |
| **Decided** in an issue | A `decision` issue of cesiumlang/cesium (cesiumlang/cesium#4 to #11, #14), or the reference where it records one. Waiting to be carried by a proposal. |
| **Direction** in an issue, with its date | An answer emanspeaks gave on a design issue (2026-11-22 to 24). A direction, not a decision: a proposal carries it later. |
| **By use** | What every example in the reference and the S0 design does, with no record behind it. |
| **Provisional** | A recommendation of the specification's editors, not a direction. Replaced by whatever is decided. |

What is still open is stated as an **Open** point in narrative, with the issue that tracks it, and no rule.

### Grammar notation

Grammars are EBNF:

| Notation | Meaning |
| --- | --- |
| `name = ... ;` | A production |
| `"text"` | A terminal: the characters as written |
| `a b` | `a` followed by `b` |
| `a \| b` | `a` or `b` |
| `[ a ]` | An optional `a` |
| `{ a }` | Zero or more `a` |
| `( a )` | Grouping |
| `(* ... *)` | A comment |

## Chapters

| Chapter | Covers | Status |
| --- | --- | --- |
| [Lexical structure](lexical.md) | Source files, tokens, comments, identifiers, keywords, literals, operators | Draft |
| [The S0 subset](s0.md) | The grammar and rules of S0, the language of stage 0 | Draft |
| Types, expressions, statements, declarations, ownership | The full language | Not written; in the [reference](https://cesiumlang.org/reference/) and the design issues of cesiumlang/cesium |

## Conformance levels

%rule[conf.levels static draft introduced-by=P-0000] The bootstrap chain defines nested levels of the language. Each compiler accepts its level and every level below it. Decided in [#9]:

| Level | Defined by | Accepted by |
| --- | --- | --- |
| S0 | [The S0 subset](s0.md) | Stage 0 (`cesium0`) and every later compiler |
| S1 | Not written | `cesium1` and every later compiler |
| Full | The whole specification | The real compiler |

%rule[conf.subset static draft introduced-by=P-0000] Each level is a strict subset of the next: a program of one level is a program of every higher level, with the same meaning. Decided in [#9], rule 1.

Open: the S1 feature list ([#76]); which features of the full language are core, planned or unproven ([#58]).

[#9]: https://github.com/cesiumlang/cesium/issues/9
[#58]: https://github.com/cesiumlang/cesium/issues/58
[#76]: https://github.com/cesiumlang/cesium/issues/76
