//! cesium1 (bootstrap/stage1), written in S0 and built by cesium0.

const std = @import("std");
const link = @import("link.zig");

/// cesium1's files, in the order they make one unit.
const files = [_][]const u8{ "util.csm", "lex.csm", "parse.csm", "types.csm", "habit.csm", "lower.csm", "decl.csm", "loc.csm", "main.csm" };

pub const Stage1 = struct {
    cesium1: *std.Build.Step.Compile,
    /// Its sources, in unit order.
    sources: [files.len]std.Build.LazyPath,
};

/// Compiles cesium1's sources with cesium0 and links the result.
pub fn add(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    cesium0: *std.Build.Step.Compile,
    csrt: *std.Build.Step.Compile,
    csmir: *std.Build.Step.Compile,
    devkit: []const u8,
) Stage1 {
    var sources: [files.len]std.Build.LazyPath = undefined;
    for (files, &sources) |file, *source| source.* = b.path(b.fmt("bootstrap/stage1/{s}", .{file}));
    const compile = b.addRunArtifact(cesium0);
    compile.addArg("-o");
    const assembly = compile.addOutputFileArg("cesium1.s");
    for (sources) |source| compile.addFileArg(source);
    const cesium1 = link.linkCompiler(b, target, csrt, csmir, devkit, "cesium1", assembly);
    return .{ .cesium1 = cesium1, .sources = sources };
}
