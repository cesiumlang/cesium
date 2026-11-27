//! The bootstrap's lists of source files (cesiumlang/cesium#134): one name a
//! line in bootstrap/<dir>/sources.txt, a line starting with `#` a comment.
//! The build reads them here, tools/portable-c.sh and the fuzzer
//! (tools/fuzz-stage0/corpus.zig) read the same files, and `zig build test`
//! checks them against their directories and the README.

const std = @import("std");

pub const Error = std.Io.Dir.OpenError || std.Io.Dir.ReadFileAllocError || std.Io.Dir.Iterator.Error ||
    std.mem.Allocator.Error;

/// The names of bootstrap/`dir`/sources.txt, in its order. The build
/// configures again when the list or the directory changes.
pub fn read(b: *std.Build, dir: []const u8) Error![]const []const u8 {
    const path = b.fmt("bootstrap/{s}", .{dir});
    b.dependOnFileContents(b.path(b.fmt("{s}/sources.txt", .{path})));
    b.dependOnDirectoryContents(b.path(path));
    const io = b.graph.io;
    const d = try b.root.openDir(io, path, .{});
    defer d.close(io);
    const text = try d.readFileAlloc(io, "sources.txt", b.allocator, .limited(1 << 16));
    var names: std.ArrayList([]const u8) = .empty;
    var lines = std.mem.splitScalar(u8, text, '\n');
    while (lines.next()) |raw| {
        const line = std.mem.trim(u8, raw, " \t\r");
        if (line.len == 0 or line[0] == '#') continue;
        try names.append(b.allocator, line);
    }
    return names.items;
}

/// Fails `step` unless the files of bootstrap/`dir` ending in `ext` are
/// exactly `names`.
pub fn check(b: *std.Build, step: *std.Build.Step, dir: []const u8, ext: []const u8, names: []const []const u8) Error!void {
    const io = b.graph.io;
    var d = try b.root.openDir(io, b.fmt("bootstrap/{s}", .{dir}), .{ .iterate = true });
    defer d.close(io);
    var problems: std.ArrayList(u8) = .empty;
    var on_disk: std.ArrayList([]const u8) = .empty;
    var it = d.iterate();
    while (try it.next(io)) |entry| {
        if (entry.kind != .file or !std.mem.endsWith(u8, entry.name, ext)) continue;
        const name = b.dupe(entry.name);
        try on_disk.append(b.allocator, name);
        if (!contains(names, name)) {
            try problems.appendSlice(b.allocator, b.fmt("bootstrap/{s}/{s} is not in bootstrap/{s}/sources.txt\n", .{ dir, name, dir }));
        }
    }
    for (names) |name| {
        if (!contains(on_disk.items, name)) {
            try problems.appendSlice(b.allocator, b.fmt("bootstrap/{s}/sources.txt names {s}, which is not there\n", .{ dir, name }));
        }
    }
    fail(b, step, problems.items);
}

/// Fails `step` unless the README's table of `dir` has a row for each of
/// `names` (| `name` | ...).
pub fn checkTable(b: *std.Build, step: *std.Build.Step, dir: []const u8, names: []const []const u8) Error!void {
    const io = b.graph.io;
    b.dependOnFileContents(b.path("bootstrap/README.md"));
    const readme = try b.root.openDir(io, "bootstrap", .{});
    defer readme.close(io);
    const text = try readme.readFileAlloc(io, "README.md", b.allocator, .limited(1 << 20));
    var problems: std.ArrayList(u8) = .empty;
    for (names) |name| {
        if (std.mem.find(u8, text, b.fmt("| `{s}` |", .{name})) == null) {
            try problems.appendSlice(b.allocator, b.fmt("bootstrap/README.md has no row for {s}/{s} in its table\n", .{ dir, name }));
        }
    }
    fail(b, step, problems.items);
}

fn contains(names: []const []const u8, name: []const u8) bool {
    for (names) |n| {
        if (std.mem.eql(u8, n, name)) return true;
    }
    return false;
}

fn fail(b: *std.Build, step: *std.Build.Step, problems: []const u8) void {
    if (problems.len == 0) return;
    const f = b.addFail(problems);
    f.step.name = "check the source lists";
    step.dependOn(&f.step);
}
