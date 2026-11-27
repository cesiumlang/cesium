//! cesium1 (bootstrap/stage1), written in S0 and built by cesium0.

const std = @import("std");
const link = @import("link.zig");

pub const Stage1 = struct {
    cesium1: *std.Build.Step.Compile,
    /// Its sources, in unit order.
    sources: []const std.Build.LazyPath,
};

/// Compiles cesium1's sources, `files` of bootstrap/stage1 in the order they
/// make one unit (bootstrap/stage1/sources.txt), with cesium0 in the build's
/// mode, which keeps its variables for debuggers in a debug build, and links
/// the result.
pub fn add(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    files: []const []const u8,
    cesium0: *std.Build.Step.Compile,
    csrt: *std.Build.Step.Compile,
    csir: *std.Build.Step.Compile,
    devkit: []const u8,
) std.mem.Allocator.Error!Stage1 {
    const sources = try b.allocator.alloc(std.Build.LazyPath, files.len);
    for (files, sources) |file, *source| source.* = b.path(b.fmt("bootstrap/stage1/{s}", .{file}));
    const compile = b.addRunArtifact(cesium0);
    compile.addArgs(&.{ "-O", @tagName(optimize), "-o" });
    const assembly = compile.addOutputFileArg("cesium1.s");
    for (sources) |source| compile.addFileArg(source);
    const cesium1 = link.linkCompiler(b, target, csrt, csir, devkit, "cesium1", assembly);
    return .{ .cesium1 = cesium1, .sources = sources };
}
