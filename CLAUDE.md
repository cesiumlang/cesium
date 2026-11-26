# Cesium

The compiler of the Cesium language: stage 0 (`bootstrap/`), the specification (`spec/`), the proposals (`proposals/`) and the charter (`docs/charter.md`). In the cesium workspace, its `CLAUDE.md` and skills (`cesium-language`, `cesium-spec`) apply too.

## How the design is decided

The process is Proposal 0000 (`proposals/0000-documentation-system.md`):

- **The charter and the proposals decide.** A proposal is `proposals/NNNN-name.md` (the charter is `docs/charter.md`, P-0001), with the status `draft`, `proposed`, `accepted`, `rejected`, `withdrawn` or `superseded`. Anyone, agents included, may write one and set `proposed`; only emanspeaks sets `accepted`, `rejected` or `superseded`, by merging its pull request. An agent never sets `accepted`.
- **The specification records.** Every rule in `spec/` is `draft` until an accepted proposal makes it `final`, and its text ends with its source: decided in an issue or the reference, a direction of emanspeaks with its date, by use, or provisional. What is still open is an "Open:" paragraph with its issue, never a rule.
- **The tests cite.** A test names the rules it checks (`// spec: ID, ID`); citations are reviewed (`skein review FILE`). The specification's examples are compiled by `cesium0` (`bash tools/spec-examples.sh`).
- **Issues** track the work and the open questions (`design`, `audit`); never close or decide an open design question.

Check with `skein check` (Skein from the VS Code extension `chasingdaylight.skein`, or `bash tools/fetch-skein.sh`); CI runs it, the examples and the coverage report.

<!-- skein: begin; written by 'skein agents', which writes it again -->

## Skein

This repository keeps its requirements, specifications and their traceability with Skein (<https://github.com/chasingdaylight/skein>). An item is a paragraph or list entry of a Markdown document, or a Markdown file of its own, with a marker:

```markdown
%TYPE[ID words key=value,value] The text of the item.

- %TYPE[.part] A part of the item above, as a list entry.
```

- A marker is `%`, the item type, and in brackets the ID, then bare words (the category and the status) and `key=value` (an attribute, or links to the items named, separated by commas). `.part` names a part by its whole's ID.
- `%[ID]` in text refers to an item; `%[ID some words]` shows the words in its place.
- IDs never change by hand: `skein move OLD NEW` renames one everywhere. `skein new TYPE` starts an item, `skein link FROM TYPE TO` adds a link.
- Items are in `proposals/` (a file per item), `spec/`.

Item types:

- `example`: Cesium code that shows a rule: accepted, or rejected; categories `accepts`, `rejects`; statuses `draft`, `final`, `deprecated`, `removed`.
- `proposal`: A design change, decided by emanspeaks when its pull request merges; statuses `draft`, `proposed`, `accepted`, `rejected`, `withdrawn`, `superseded`.
- `rule`: A rule of the language; final only through an accepted proposal; categories `syntax`, `legality`, `static`, `dynamic`; statuses `draft`, `final`, `deprecated`, `removed`.
- `term`: A term the specification defines, for its glossary; statuses `draft`, `final`, `deprecated`, `removed`.
- `test`: A conformance test kept as an item: one carried out by hand, or described before it is written in code; statuses `draft`, `approved`, `deprecated`, `removed`.

Link types (`key=ID` in a marker):

- `changed-by` from `rule`, `term`, `example` to `proposal`: A proposal that amended a rule, term or example after it was introduced.
- `introduced-by` from `rule`, `term` to `proposal`: The proposal that brought a rule or term into the specification; an example goes with its rule (required).
- `see` from `rule` to `rule`, `term`: A rule refers to another rule, or to a term.
- `shows` from `example` to `rule`: An example shows a rule.
- `superseded-by` from `proposal` to `proposal`: The proposal that replaces this one.
- `verifies` from `test` to `rule`: A conformance test verifies a rule.

Code and tests cite items in comments:

- `test`: files under `bootstrap/tests/` ending in `.csm`, `// spec: ID, ID`.

After editing documents, item files or `.skein/`, run `skein check` and mend what it reports. `skein trace ID` shows an item with everything above and below it, and `skein coverage` what is not yet linked or cited. `skein mcp` offers all of this as tools to agents that speak the Model Context Protocol.

<!-- skein: end -->
