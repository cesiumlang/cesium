//! The differential corpus (bootstrap/tests/diff), as the step test-diff:
//! generated S0 programs that cesium1 must compile as cesium0 does.
//!
//!   diff/<name>.csm          cesium1's core SIR, from standard input, must be
//!                            cesium0's without its loc lines
//!   diff/errors/<name>.csm   both must exit with status 1, and the first
//!                            error of each must be the same code and message
//!                            at the same place
//!
//! The files are found by listing the two directories.

const std = @import("std");
const tests = @import("tests.zig");

const diff_dir = "bootstrap/tests/diff";

pub const Error = std.Io.Dir.OpenError || std.Io.Dir.Iterator.Error || std.mem.Allocator.Error;

pub fn add(
    b: *std.Build,
    step: *std.Build.Step,
    target: std.Build.ResolvedTarget,
    tool: *std.Build.Step.Compile,
    cesium0: *std.Build.Step.Compile,
    cesium1: *std.Build.Step.Compile,
) Error!void {
    const crlf = tests.writesCrlf(target);
    for (try csmFiles(b, diff_dir)) |file| {
        const name = std.fs.path.stem(file);
        const source = b.path(b.fmt(diff_dir ++ "/{s}", .{file}));
        const emit = b.addRunArtifact(cesium0);
        emit.setName(b.fmt("cesium0 diff-{s}", .{name}));
        emit.addArgs(&.{ "--emit-sir", "-o" });
        const reference = emit.addOutputFileArg(b.fmt("{s}.sir", .{name}));
        emit.addFileArg(source);
        tests.checkSirOf(b, step, tool, crlf, cesium1, b.fmt("diff-{s}", .{name}), source, reference);
    }
    for (try csmFiles(b, diff_dir ++ "/errors")) |file| {
        const name = std.fs.path.stem(file);
        const source = b.path(b.fmt(diff_dir ++ "/errors/{s}", .{file}));

        const run0 = b.addRunArtifact(cesium0);
        run0.setName(b.fmt("cesium0 rejects diff-{s}", .{name}));
        run0.addArgs(&.{ "--emit-sir", "-o" });
        _ = run0.addOutputFileArg(b.fmt("{s}.sir", .{name}));
        run0.addFileArg(source);
        run0.expectExitCode(1);
        const error0 = run0.captureStdErr(.{ .basename = b.fmt("{s}.cesium0.txt", .{name}) });

        const run1 = b.addRunArtifact(cesium1);
        run1.setName(b.fmt("cesium1 rejects diff-{s}", .{name}));
        run1.setStdIn(.{ .lazy_path = source });
        run1.expectExitCode(1);
        const error1 = run1.captureStdOut(.{ .basename = b.fmt("{s}.cesium1.txt", .{name}) });

        const same = b.addRunArtifact(tool);
        same.setName(b.fmt("compare diff-{s} error with cesium0", .{name}));
        same.addArg("error");
        same.addFileArg(error0);
        same.addFileArg(error1);
        same.expectExitCode(0);
        step.dependOn(&same.step);
    }
}

/// The .csm files of a directory of the source tree, sorted by name. A file
/// added or removed there makes the build configure again.
fn csmFiles(b: *std.Build, sub_path: []const u8) Error![]const []const u8 {
    b.dependOnDirectoryContents(b.path(sub_path));
    const io = b.graph.io;
    var dir = try b.root.openDir(io, sub_path, .{ .iterate = true });
    defer dir.close(io);
    var files: std.ArrayList([]const u8) = .empty;
    var it = dir.iterate();
    while (try it.next(io)) |entry| {
        if (entry.kind == .file and std.mem.endsWith(u8, entry.name, ".csm")) {
            try files.append(b.allocator, b.dupe(entry.name));
        }
    }
    std.mem.sort([]const u8, files.items, {}, lessThan);
    return files.items;
}

fn lessThan(_: void, x: []const u8, y: []const u8) bool {
    return std.mem.lessThan(u8, x, y);
}
