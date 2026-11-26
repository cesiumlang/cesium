//! The check that holds bootstrap/diagnostics.md and the two compilers
//! together (cesiumlang/cesium#82), run by `zig build test`:
//!
//!   diagnostics <catalog> <source>...
//!
//! Every string literal "E" + four digits in a source (`.c`: cesium0, `.csm`:
//! cesium1) must be a code of the catalog (a heading `### E0101`), and every
//! code of the catalog must appear in both compilers, or only in the one its
//! entry names with `cesium0 only` or `cesium1 only`. Each problem is
//! printed, and the exit status is 1.

const std = @import("std");

const compilers = [_][]const u8{ "cesium0", "cesium1" };
const markers = [_][]const u8{ "cesium0 only", "cesium1 only" };

/// A code of the catalog: the compiler it is limited to, if any, and the
/// first file of each compiler that uses it.
const Entry = struct {
    only: ?usize = null,
    used: [2]?[]const u8 = .{ null, null },
};

pub const Error = std.Io.Dir.ReadFileAllocError || std.process.Args.ToSliceError;

/// Adds the check to `step`: the catalog against the files of bootstrap/stage0
/// named in `stage0` and cesium1's sources.
pub fn add(b: *std.Build, step: *std.Build.Step, stage0: []const []const u8, stage1: []const std.Build.LazyPath) void {
    const exe = b.addExecutable(.{
        .name = "diagnostics",
        .root_module = b.createModule(.{ .root_source_file = b.path("build-src/diagnostics.zig"), .target = b.graph.host }),
    });
    const run = b.addRunArtifact(exe);
    run.setName("check the diagnostic codes");
    run.addFileArg(b.path("bootstrap/diagnostics.md"));
    for (stage0) |file| run.addFileArg(b.path(b.fmt("bootstrap/stage0/{s}", .{file})));
    for (stage1) |source| run.addFileArg(source);
    run.expectExitCode(0);
    step.dependOn(&run.step);
}

pub fn main(init: std.process.Init) Error!u8 {
    const io = init.io;
    const a = init.arena.allocator();
    const argv = try init.minimal.args.toSlice(a);
    if (argv.len < 3) {
        std.debug.print("usage: diagnostics <catalog> <source>...\n", .{});
        return 2;
    }
    const cwd = std.Io.Dir.cwd();
    const entries = try a.alloc(?Entry, 10000);
    @memset(entries, null);
    var failed = false;

    // the catalog: a heading per code, then its entry
    var current: ?*Entry = null;
    var lines = std.mem.splitScalar(u8, try cwd.readFileAlloc(io, argv[1], a, .limited(1 << 24)), '\n');
    while (lines.next()) |raw| {
        const line = std.mem.trimEnd(u8, raw, "\r");
        if (std.mem.startsWith(u8, line, "#")) {
            current = null;
            if (!std.mem.startsWith(u8, line, "### ")) continue;
            const n = code(line[4..]) orelse continue;
            if (entries[n] != null) {
                std.debug.print("E{d:0>4} has two entries in the catalog\n", .{n});
                failed = true;
            }
            entries[n] = .{};
            current = &entries[n].?;
        } else if (current) |e| {
            for (markers, 0..) |marker, i| {
                if (std.mem.find(u8, line, marker) != null) e.only = i;
            }
        }
    }

    // the sources: every "Ennnn" literal
    for (argv[2..]) |path| {
        const which: usize = if (std.mem.endsWith(u8, path, ".csm")) 1 else 0;
        const file = std.Io.Dir.path.basename(path);
        const text = try cwd.readFileAlloc(io, path, a, .limited(1 << 24));
        var start: usize = 0;
        while (std.mem.findPos(u8, text, start, "\"E")) |at| : (start = at + 1) {
            if (at + 7 > text.len or text[at + 6] != '"') continue;
            const n = code(text[at + 1 .. at + 6]) orelse continue;
            if (entries[n]) |*e| {
                if (e.used[which] == null) e.used[which] = file;
            } else {
                std.debug.print("E{d:0>4} in {s} is not in the catalog\n", .{ n, file });
                failed = true;
            }
        }
    }

    for (entries, 0..) |entry, n| {
        const e = entry orelse continue;
        if (e.used[0] == null and e.used[1] == null) {
            std.debug.print("E{d:0>4} of the catalog is in no source\n", .{n});
            failed = true;
            continue;
        }
        for (compilers, 0..) |name, i| {
            const other = 1 - i;
            if (e.used[i] == null and e.only != other) {
                std.debug.print("E{d:0>4} is in {s} ({s}) but not in {s}\n", .{ n, compilers[other], e.used[other].?, name });
                failed = true;
            } else if (e.used[i] != null and e.only == other) {
                std.debug.print("E{d:0>4} is {s} only but in {s} ({s})\n", .{ n, compilers[other], name, e.used[i].? });
                failed = true;
            }
        }
    }
    return if (failed) 1 else 0;
}

/// The number of a code, "E" and four digits, or null.
fn code(text: []const u8) ?usize {
    if (text.len != 5 or text[0] != 'E') return null;
    var n: usize = 0;
    for (text[1..]) |c| {
        if (c < '0' or c > '9') return null;
        n = n * 10 + (c - '0');
    }
    return n;
}
