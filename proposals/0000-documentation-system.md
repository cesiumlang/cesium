+++
id = "P-0000"
type = "proposal"
status = "proposed"
+++

# Proposal 0000: one documentation system

This proposal sets how Cesium's design is written down, decided and checked. It is the first proposal and describes the process that every later one follows. It is accepted when emanspeaks merges the pull request that carries it; until then nothing in it is decided.

It carries out cesiumlang/cesium-workspace#23.

## Motivation

The design is spread over several places: issues labeled `decision` (cesiumlang/cesium#4 to #11), long threads on design issues, reference pages and a draft specification on cesiumlang.org, and a draft S0 grammar. Three things follow from that:

- It is unclear which points emanspeaks has actually accepted. The `decision` issues are summaries written by an agent.
- The threads are expensive to read, for people and for agents.
- Nothing connects the specification to the tests, so nobody can say which rules are tested.

## The system

Five layers, each with one job.

| Layer | Holds | Where | Decides anything? |
| --- | --- | --- | --- |
| Charter | Goals, non-goals and design principles. Short and stable. | `docs/charter.md` in cesiumlang/cesium | Yes, through a proposal |
| Proposals | One numbered document per design change. | `proposals/` in cesiumlang/cesium | Yes: the only way something becomes decided |
| Specification | The normative rules, each with a stable ID. | `spec/` in cesiumlang/cesium | No: it records what proposals decided |
| Conformance suite | Valid and invalid programs, each citing the rules it checks. | `bootstrap/tests/` and later `conformance/` in cesiumlang/cesium | No |
| User documentation and compiler notes | What is, for users and contributors. | cesiumlang.org | No |

Issues stay for tasks, bugs and the order of work in the Cesium Roadmap project. An open design issue becomes a proposal when it is worked, and the issue then only tracks the proposal.

### Why the specification moves into the compiler repository

The specification is on cesiumlang.org today. The tests are in cesiumlang/cesium. A rule and the test that checks it have to be in one tree to be checked against each other in one CI run, so the specification moves next to the tests. The website already builds from a checkout of the compiler repository (`build.js` finds `../cesium-src`), so it renders the specification from there, and readers see no difference.

### Proposals

A proposal is a Markdown file `proposals/NNNN-short-name.md` with these sections: motivation, design, alternatives, drawbacks, open questions. Its status is one of:

| Status | Meaning |
| --- | --- |
| `draft` | Being written. Not yet ready for a decision. |
| `proposed` | Ready. Waiting for emanspeaks. |
| `accepted` | Decided. The specification is updated to match. |
| `rejected` | Decided against. Kept for the record. |
| `withdrawn` | Dropped by its author before a decision. |
| `superseded` | Replaced by a later proposal, which it names. |

A proposal is discussed on its pull request. Merging that pull request with the status `accepted` is the decision, and only emanspeaks does that. An agent may write a proposal and recommend an answer; it never sets `accepted`.

### The specification

The specification is Markdown, one chapter per file, read by [skein](https://github.com/chasingdaylight/skein). A rule is a paragraph or list entry with a marker:

```markdown
%rule[lex.comment.line syntax final introduced-by=P-0012] A line comment starts with `//` and ends at the end of the line.

%rule[lex.keywords syntax] The keywords are:

- %rule[.as] `as`
- %rule[.fn] `fn`
```

- Every rule has an ID that says what it is about and never changes. A rule that is dropped keeps its ID, with the status `removed`.
- Every rule links to the proposal that introduced it (`introduced-by`).
- Text without a marker is narrative: it explains and decides nothing.
- A rule refers to another rule or to a defined term with `%[id]`.

The statuses of a rule:

| Status | Meaning |
| --- | --- |
| `draft` | Written down, not yet accepted through a proposal. Guidance, not settled fact. |
| `final` | Accepted through a proposal. |
| `deprecated` | Still in force, and scheduled to go. |
| `removed` | No longer a rule. The ID stays reserved. |

Everything in today's draft specification and S0 grammar starts as `draft`. A rule becomes `final` only through an accepted proposal.

### Tests cite rules

A test names the rules it checks in a comment, and skein reads those citations:

```text
// spec: lex.comment.line, lex.comment.block
```

`skein check` fails CI when a citation or a reference names a rule that does not exist. `skein coverage` reports which rules have no test. It reports and does not fail, so coverage can grow chapter by chapter (cesiumlang/cesium#57, step 3).

### What happens to the existing material

- **The `decision` issues (cesiumlang/cesium#4 to #11):** each is rewritten as a proposal with the status `proposed`. emanspeaks reviews each point by point: accept, change, or mark open. Until then they are guidance. The "ADR" naming goes.
- **The draft specification and the S0 grammar:** moved to `spec/` with rule IDs, all `draft`.
- **The reference pages on cesiumlang.org:** stay as user documentation. Where one states something that no rule states, that statement becomes a `draft` rule or an open question in a proposal.
- **Open design issues (label `design`):** unchanged until worked. None is closed or decided by this proposal.

## Alternatives

- **Keep deciding in issues.** No tooling to add. Rejected here because it is the current state, with the three problems above.
- **A separate repository for proposals and the specification.** Cleaner history per kind of document. Rejected here because rules and tests could then not be checked against each other in one CI run until skein can cite across repositories, which is planned and not built.
- **Keep the specification on cesiumlang.org.** No move needed. Rejected here for the same reason.
- **Sphinx with sphinx-needs, or Doorstop.** Mature tools. Both need Python, which no Cesium repository may depend on (cesiumlang/cesium#90), and neither reads rules written as plain paragraphs in a Markdown document.

## Drawbacks

- skein is new and has one maintainer. It does not render HTML itself yet (chasingdaylight/skein#51 to #54); until then the website gets the specification from `skein render`, which writes Markdown with anchors and links for the site generator.
- CI in cesiumlang/cesium depends on a released skein, which it downloads.
- Moving the specification changes the paths that existing links to the website's sources point at. The published URLs can stay the same.

## Questions and their answers

Three were answered by emanspeaks on 2026-11-21. Two keep their recommendation; merging this proposal accepts them as written, and they can be changed on its pull request before that.

1. **Location: this repository.** The charter, the proposals and the specification live in cesiumlang/cesium. It is the hub of everything else, and all the documentation and the specification of the language stay with the compiler's source; the workspace repository is for developers' tooling only. Everything is driven by the pipelines of this repository. If another repository ever needs items of its own, it gets a `.skein/` directory too, and skein links the two (planned there as links across repositories); nothing needs that now.
2. **Rule categories:** `syntax`, `legality` (what the compiler must reject), `static` (meaning fixed at compile time) and `dynamic` (behavior at run time), following the Ferrocene Language Specification.
3. **How skein is obtained.** Not through the buildtools. Developers get it with the VS Code extension `chasingdaylight.skein`, which is to carry the `skein` program for the platform (chasingdaylight/skein#74). CI downloads the archive of a pinned skein release for the runner and checks it against the release's checksums.
4. **Proposal numbers** (recommendation): four digits in order of filing, `P-0001` onward, with `P-0001` the charter.
5. **Who may set `proposed`** (recommendation): anyone, agents included; only emanspeaks sets `accepted` or `rejected`.

## After acceptance

In this order, each as its own pull request:

1. `.skein/` in cesiumlang/cesium with the item types `proposal`, `rule` and `term`, the link type `introduced-by`, and a citation set for the tests; `skein check` in CI.
2. `P-0001`, the charter, from the goals on cesiumlang.org and the 2025 design.
3. The `decision` issues as proposals `P-0002` onward.
4. The specification in `spec/` with rule IDs, lexical structure and the S0 grammar first.
5. Citations in the bootstrap tests, and the coverage report in CI.
6. `CLAUDE.md`, the workspace skills, the Roadmap project's readme and the agent plugin's skill describe the process.
