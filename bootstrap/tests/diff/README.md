# The generated corpus

S0 programs generated during the work on [cesiumlang/cesium#62](https://github.com/cesiumlang/cesium/issues/62) to compare `cesium1` with `cesium0`. The generator was not kept; the files are kept as they are and are not regenerated.

- `c<nnn>.csm`: 175 generated programs of 24 lines: the same declarations in each, and a different one-line body of `main`.
- `t<nnn>.csm`: 71 small programs, each aimed at one rule.

Programs both compilers accept are here (97); programs both reject are in `errors/` (149). `zig build test-diff` checks that `cesium1` gives the core SIR `cesium0 --emit-sir` gives for each accepted program, and that for each rejected one both exit with status 1 and give the same first error at the same place. Decided in item 3 of [cesiumlang/cesium#67](https://github.com/cesiumlang/cesium/issues/67): the step is not part of `zig build test`, and CI runs it on one host.
