//! The corpus the fuzzer damages: the test files of bootstrap/tests and
//! cesium1's source, each with how cesium0 is run on it, and the words a
//! token may become.

const std = @import("std");
const tokens = @import("tokens.zig");
const mutate = @import("mutate.zig");
const Io = std.Io;
const Allocator = std.mem.Allocator;

pub const Kind = enum { mir, s0 };

/// One corpus file and how cesium0 is run on a mutant of it.
pub const Seed = struct {
    /// Relative to the repository root, for the report.
    name: []const u8,
    kind: Kind,
    data: []const u8,
    /// The files of the same unit before and after it (S0 only).
    before: []const []const u8 = &.{},
    after: []const []const u8 = &.{},

    /// The command line of cesium0 on `mutant`.
    pub fn command(seed: Seed, a: Allocator, cesium0: []const u8, emit: []const u8, out: []const u8, mutant: []const u8) Allocator.Error![]const []const u8 {
        var argv: std.ArrayList([]const u8) = .empty;
        try argv.appendSlice(a, &.{ cesium0, emit, "-o", out });
        switch (seed.kind) {
            .mir => try argv.appendSlice(a, &.{ "--mir", mutant }),
            .s0 => {
                try argv.appendSlice(a, seed.before);
                try argv.append(a, mutant);
                try argv.appendSlice(a, seed.after);
            },
        }
        return argv.items;
    }
};

pub const Error = Io.Dir.OpenError || Io.Dir.Iterator.Error || Io.Dir.ReadFileAllocError || error{NoCorpus};

const stage1 = [_][]const u8{ "util.csm", "lex.csm", "parse.csm", "types.csm", "lower.csm", "decl.csm", "main.csm" };

/// The corpus under `root`/bootstrap, `root` an absolute path.
pub fn load(a: Allocator, io: Io, root: []const u8) Error![]const Seed {
    var seeds: std.ArrayList(Seed) = .empty;
    const mir_files = try list(a, io, root, &.{ "bootstrap/tests/mir", "bootstrap/tests/mir/errors", "bootstrap/tests/s0" }, ".mir");
    for (mir_files) |name| {
        try seeds.append(a, .{ .name = name, .kind = .mir, .data = try read(a, io, root, name) });
    }
    const lib = try absolute(a, root, "bootstrap/tests/s0/lib.csm");
    const s0_files = try list(a, io, root, &.{ "bootstrap/tests/s0", "bootstrap/tests/s0/errors" }, ".csm");
    for (s0_files) |name| {
        const base = std.fs.path.basenamePosix(name);
        const alone = std.mem.eql(u8, base, "tokens.csm") or std.mem.eql(u8, base, "lib.csm");
        try seeds.append(a, .{
            .name = name,
            .kind = .s0,
            .data = try read(a, io, root, name),
            .after = if (alone) &.{} else try a.dupe([]const u8, &.{lib}),
        });
    }
    // cesium1's source: each file in turn is the damaged one
    var unit: [stage1.len][]const u8 = undefined;
    for (stage1, &unit) |file, *path| path.* = try absolute(a, root, try a.print("bootstrap/stage1/{s}", .{file}));
    const all = try a.dupe([]const u8, &unit);
    for (stage1, 0..) |file, i| {
        const name = try a.print("bootstrap/stage1/{s}", .{file});
        try seeds.append(a, .{ .name = name, .kind = .s0, .data = try read(a, io, root, name), .before = all[0..i], .after = all[i + 1 ..] });
    }
    if (seeds.items.len == 0) return error.NoCorpus;
    return seeds.items;
}

/// Every distinct token of the corpus files of `kind`, sorted, then the
/// interesting ones.
pub fn words(a: Allocator, seeds: []const Seed, kind: Kind) Allocator.Error![]const []const u8 {
    var found: std.ArrayList([]const u8) = .empty;
    for (seeds) |seed| {
        if (seed.kind != kind) continue;
        for (try tokens.split(a, seed.data)) |token| {
            if (!tokens.isWhitespace(token)) try found.append(a, token);
        }
    }
    std.mem.sort([]const u8, found.items, {}, lessThan);
    var out: std.ArrayList([]const u8) = .empty;
    for (found.items) |token| {
        if (out.items.len == 0 or !std.mem.eql(u8, out.items[out.items.len - 1], token)) try out.append(a, token);
    }
    try out.appendSlice(a, &mutate.interesting);
    return out.items;
}

/// The files of `dirs` (relative to `root`) whose names end in `ext`, as
/// relative paths, sorted.
fn list(a: Allocator, io: Io, root: []const u8, dirs: []const []const u8, ext: []const u8) Error![]const []const u8 {
    var names: std.ArrayList([]const u8) = .empty;
    var top = try Io.Dir.cwd().openDir(io, root, .{});
    defer top.close(io);
    for (dirs) |sub| {
        var dir = try top.openDir(io, sub, .{ .iterate = true });
        defer dir.close(io);
        var it = dir.iterate();
        while (try it.next(io)) |entry| {
            if (entry.kind == .file and std.mem.endsWith(u8, entry.name, ext)) {
                try names.append(a, try a.print("{s}/{s}", .{ sub, entry.name }));
            }
        }
    }
    std.mem.sort([]const u8, names.items, {}, lessThan);
    return names.items;
}

fn read(a: Allocator, io: Io, root: []const u8, name: []const u8) Error![]const u8 {
    return Io.Dir.cwd().readFileAlloc(io, try absolute(a, root, name), a, .limited(16 << 20));
}

fn absolute(a: Allocator, root: []const u8, name: []const u8) Allocator.Error![]const u8 {
    return a.print("{s}/{s}", .{ root, name });
}

fn lessThan(_: void, x: []const u8, y: []const u8) bool {
    return std.mem.lessThan(u8, x, y);
}
