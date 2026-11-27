//! Mutation fuzzing of cesium0's two readers: the SIR text parser and the S0
//! front end (cesiumlang/cesium#10, "consider fuzzing the SIR text parser").
//!
//! Takes the files of the test corpus, damages each a little, and runs
//! cesium0 on the result up to QBE IL. Whatever the input, cesium0 must exit
//! with status 0 (accepted) or 1 (rejected with a message), within the time
//! limit, and without a sanitizer report. SIR text that cesium0 accepts and
//! prints must also parse again and print the same.
//!
//! Run it on a cesium0 built with sanitizers: tools/portable-c.sh fuzz
//! <compiler> does both. The inputs depend only on --seed and the iteration
//! number, so a failure can be reproduced; every failing input is kept in the
//! output directory next to a .txt file with the command and what went wrong.
//!
//!   zig build fuzz -- [<cesium0>] [--iterations N] [--seed S] [--out DIR]
//!                     [--timeout SECONDS] [--jobs N]
//!
//! Without <cesium0>, the build's own cesium0 is fuzzed. It runs at the root
//! of the repository.

const std = @import("std");
const corpus = @import("corpus.zig");
const iteration = @import("iteration.zig");
const Io = std.Io;
const Allocator = std.mem.Allocator;

const usage =
    \\usage: fuzz-stage0 <cesium0> [--iterations N] [--seed S] [--out DIR]
    \\                   [--timeout SECONDS] [--jobs N]
    \\
;

/// Stop after this many failing inputs.
const max_failures = 20;

const Options = struct {
    cesium0: ?[]const u8 = null,
    /// The one to fuzz without <cesium0>: the build's own (build-src/fuzz.zig).
    default_cesium0: ?[]const u8 = null,
    iterations: u64 = 2000,
    seed: u64 = 1,
    out: []const u8 = "tmp/fuzz",
    timeout_text: []const u8 = "20",
    timeout: Io.Clock.Duration = seconds(20),
    jobs: ?usize = null,
};

fn seconds(s: f64) Io.Clock.Duration {
    return .{ .raw = .fromMilliseconds(@intFromFloat(s * 1000)), .clock = .awake };
}

const ParseError = error{ MissingValue, UnknownOption, ExtraArgument, MissingCesium0 } ||
    std.fmt.ParseIntError || std.fmt.ParseFloatError;

fn parse(argv: []const []const u8) ParseError!Options {
    var o: Options = .{};
    var i: usize = 1;
    while (i < argv.len) : (i += 1) {
        const arg = argv[i];
        if (!std.mem.startsWith(u8, arg, "--")) {
            if (o.cesium0 != null) return error.ExtraArgument;
            o.cesium0 = arg;
            continue;
        }
        // --name value, or --name=value
        const eq = std.mem.findScalar(u8, arg, '=');
        const name = arg[2 .. eq orelse arg.len];
        const value = if (eq) |e| arg[e + 1 ..] else blk: {
            i += 1;
            if (i == argv.len) return error.MissingValue;
            break :blk argv[i];
        };
        if (std.mem.eql(u8, name, "iterations")) {
            o.iterations = try std.fmt.parseInt(u64, value, 10);
        } else if (std.mem.eql(u8, name, "seed")) {
            o.seed = try std.fmt.parseInt(u64, value, 10);
        } else if (std.mem.eql(u8, name, "out")) {
            o.out = value;
        } else if (std.mem.eql(u8, name, "timeout")) {
            o.timeout = seconds(try std.fmt.parseFloat(f64, value));
            o.timeout_text = value;
        } else if (std.mem.eql(u8, name, "default-cesium0")) {
            o.default_cesium0 = value;
        } else if (std.mem.eql(u8, name, "jobs")) {
            o.jobs = try std.fmt.parseInt(usize, value, 10);
        } else return error.UnknownOption;
    }
    o.cesium0 = o.cesium0 orelse o.default_cesium0 orelse return error.MissingCesium0;
    return o;
}

pub fn main(init: std.process.Init) !u8 {
    const io = init.io;
    const a = init.arena.allocator();
    const argv = try init.minimal.args.toSlice(a);
    const options = parse(argv) catch |e| {
        std.debug.print("{s}fuzz-stage0: {s}\n", .{ usage, @errorName(e) });
        return 2;
    };
    const cwd = Io.Dir.cwd();
    const jobs = @max(1, options.jobs orelse (std.Thread.getCpuCount() catch 2));
    const cesium0 = try cwd.realPathFileAlloc(io, options.cesium0.?, a);
    cwd.deleteTree(io, options.out) catch {};
    try cwd.createDirPath(io, options.out);
    const out = try cwd.realPathFileAlloc(io, options.out, a);
    const root = try cwd.realPathFileAlloc(io, ".", a);

    const seeds = try corpus.load(a, io, root);
    const ctx: iteration.Context = .{
        .io = io,
        .gpa = init.gpa,
        .keep = a,
        .cesium0 = cesium0,
        .seed = options.seed,
        .out = out,
        .timeout = options.timeout,
        .timeout_text = options.timeout_text,
        .jobs = jobs,
        .seeds = seeds,
        .sir_words = try corpus.words(a, seeds, .sir),
        .s0_words = try corpus.words(a, seeds, .s0),
    };

    var stdout_buffer: [4096]u8 = undefined;
    var stdout_writer = Io.File.stdout().writer(io, &stdout_buffer);
    const stdout = &stdout_writer.interface;

    const results = try a.alloc(?[]const u8, jobs);
    var failures: usize = 0;
    var start: u64 = 0;
    while (start < options.iterations) : (start += jobs) {
        const end = @min(start + jobs, options.iterations);
        var group: Io.Group = .init;
        for (start..end, results[0 .. end - start]) |n, *result| {
            group.concurrent(io, iteration.run, .{ &ctx, n, result }) catch
                try iteration.run(&ctx, n, result);
        }
        try group.await(io);
        for (results[0 .. end - start]) |result| {
            const report = result orelse continue;
            failures += 1;
            try stdout.print("{s}\n", .{report});
            try stdout.flush();
        }
        if (failures >= max_failures) {
            try stdout.print("stopping after {d} failures\n", .{max_failures});
            break;
        }
    }
    cwd.deleteTree(io, try a.print("{s}/work", .{out})) catch {};
    if (failures > 0) {
        try stdout.flush();
        std.debug.print("{d} failing input(s), kept in {s}\n", .{ failures, out });
        return 1;
    }
    try stdout.print("fuzz: {d} mutated inputs from {d} corpus files (seed {d}), no failures\n", .{ options.iterations, seeds.len, options.seed });
    try stdout.flush();
    return 0;
}
