//! One iteration of the fuzzer: damage a corpus file, run cesium0 on it,
//! check what it did, and keep the input when it misbehaved.

const std = @import("std");
const corpus = @import("corpus.zig");
const mutate = @import("mutate.zig");
const Io = std.Io;
const Allocator = std.mem.Allocator;

pub const Context = struct {
    io: Io,
    /// Threadsafe; each iteration makes its own arena of it.
    gpa: Allocator,
    /// Threadsafe; holds the reports of failing iterations.
    keep: Allocator,
    cesium0: []const u8,
    seed: u64,
    /// Absolute.
    out: []const u8,
    timeout: Io.Clock.Duration,
    /// The timeout as given, for messages.
    timeout_text: []const u8,
    jobs: usize,
    seeds: []const corpus.Seed,
    mir_words: []const []const u8,
    s0_words: []const []const u8,
};

pub const Error = Allocator.Error || Io.Dir.CreateDirPathError || Io.Dir.WriteFileError ||
    Io.Dir.ReadFileAllocError || std.process.RunError;

/// Run iteration `number`; `result` becomes null, or the report of the
/// failure. A group task, so it returns no errors: an error of the fuzzer
/// itself is reported as a failure.
pub fn run(ctx: *const Context, number: u64, result: *?[]const u8) Io.Cancelable!void {
    var arena: std.heap.ArenaAllocator = .init(ctx.gpa);
    defer arena.deinit();
    result.* = one(ctx, arena.allocator(), number) catch |e| switch (e) {
        error.Canceled => return error.Canceled,
        else => ctx.keep.print("iteration {d}: the fuzzer failed: {s}", .{ number, @errorName(e) }) catch "the fuzzer ran out of memory",
    };
}

/// What cesium0 did, when it misbehaved.
const Outcome = struct {
    problem: ?[]const u8 = null,
    stderr: []const u8 = "",
    status: ?u8 = null,
};

fn reported(stderr: []const u8) bool {
    return std.mem.indexOf(u8, stderr, "runtime error:") != null or std.mem.indexOf(u8, stderr, "Sanitizer") != null;
}

/// Run cesium0; whatever the input, it must exit with status 0 or 1 in time
/// and without a sanitizer report, and with a message when it rejects.
fn check(ctx: *const Context, a: Allocator, argv: []const []const u8) Error!Outcome {
    const timeout: Io.Timeout = .{ .duration = ctx.timeout };
    const done = std.process.run(a, ctx.io, .{ .argv = argv, .timeout = timeout.toDeadline(ctx.io) }) catch |e| switch (e) {
        error.Timeout => return .{ .problem = try a.print("no result within {s} s", .{ctx.timeout_text}) },
        else => |other| return other,
    };
    const status: i64 = switch (done.term) {
        .exited => |code| code,
        .signal, .stopped => |sig| -@as(i64, @backingInt(sig)),
        .unknown => |code| code,
    };
    if (reported(done.stderr)) return .{ .problem = "sanitizer report", .stderr = done.stderr };
    if (status != 0 and status != 1) {
        return .{ .problem = try a.print("exit status {d} (expected 0 or 1)", .{status}), .stderr = done.stderr };
    }
    if (status == 1 and std.mem.trim(u8, done.stderr, " \t\r\n\x0b\x0c").len == 0) {
        return .{ .problem = "exit status 1 without a message", .stderr = done.stderr, .status = 1 };
    }
    return .{ .stderr = done.stderr, .status = @intCast(status) };
}

fn one(ctx: *const Context, a: Allocator, number: u64) (Error || Io.Cancelable)!?[]const u8 {
    const io = ctx.io;
    const cwd = Io.Dir.cwd();
    var prng: std.Random.DefaultPrng = .init(ctx.seed *% 1000003 +% number);
    const r = prng.random();
    const seed = ctx.seeds[r.uintLessThan(usize, ctx.seeds.len)];
    const words = switch (seed.kind) {
        .mir => ctx.mir_words,
        .s0 => ctx.s0_words,
    };
    const data = try mutate.mutate(a, r, seed.data, words);
    // numbers are handed out in blocks of `jobs`, so that no two running
    // iterations share a work directory
    const work = try a.print("{s}/work/{d}", .{ ctx.out, number % (ctx.jobs * 4) });
    try cwd.createDirPath(io, work);
    const ext = switch (seed.kind) {
        .mir => ".mir",
        .s0 => ".csm",
    };
    const mutant = try a.print("{s}/input{s}", .{ work, ext });
    try cwd.writeFile(io, .{ .sub_path = mutant, .data = data });

    const failure: Failure = .{ .ctx = ctx, .a = a, .seed = seed, .number = number, .ext = ext, .mutant = mutant, .data = data };
    const command = try seed.command(a, ctx.cesium0, "--emit-qbe", try a.print("{s}/out.ssa", .{work}), mutant);
    const first_run = try check(ctx, a, command);
    if (first_run.problem) |p| return try failure.keep(p, command, first_run.stderr);
    if (seed.kind != .mir) return null;

    // MIR text that is accepted and printed must read back the same
    const first = try a.print("{s}/first.mir", .{work});
    const print_command = try seed.command(a, ctx.cesium0, "--emit-mir", first, mutant);
    const printed = try check(ctx, a, print_command);
    if (printed.problem) |p| return try failure.keep(p, print_command, printed.stderr);
    if (printed.status != 0) return null; // rejected
    const second = try a.print("{s}/second.mir", .{work});
    const again = try check(ctx, a, &.{ ctx.cesium0, "--emit-mir", "-o", second, "--mir", first });
    if (again.problem != null or again.status != 0) {
        return try failure.keep("the MIR text cesium0 printed for this input does not parse again", print_command, again.stderr);
    }
    const first_text = try cwd.readFileAlloc(io, first, a, .unlimited);
    const second_text = try cwd.readFileAlloc(io, second, a, .unlimited);
    if (!std.mem.eql(u8, first_text, second_text)) {
        return try failure.keep("the MIR text cesium0 printed for this input prints differently the second time", print_command, "");
    }
    return null;
}

const Failure = struct {
    ctx: *const Context,
    a: Allocator,
    seed: corpus.Seed,
    number: u64,
    ext: []const u8,
    mutant: []const u8,
    data: []const u8,

    /// Keep the input next to a .txt file with the command and what went
    /// wrong; return the report.
    fn keep(f: Failure, problem: []const u8, command: []const []const u8, stderr: []const u8) Error![]const u8 {
        const io = f.ctx.io;
        const cwd = Io.Dir.cwd();
        const kept = try f.a.print("{s}/fail-{d}-{d}{s}", .{ f.ctx.out, f.ctx.seed, f.number, f.ext });
        try cwd.writeFile(io, .{ .sub_path = kept, .data = f.data });
        var shown: std.ArrayList(u8) = .empty;
        for (command, 0..) |arg, i| {
            if (i > 0) try shown.append(f.a, ' ');
            try shown.appendSlice(f.a, if (std.mem.eql(u8, arg, f.mutant)) kept else arg);
        }
        const text = try f.a.print("{s}\nfrom {s}, seed {d}, iteration {d}\n{s}\n\n{s}", .{
            problem, f.seed.name, f.ctx.seed, f.number, shown.items, stderr,
        });
        try cwd.writeFile(io, .{ .sub_path = try f.a.print("{s}.txt", .{kept}), .data = text });
        return f.ctx.keep.print("iteration {d}: {s}: {s}", .{ f.number, problem, kept });
    }
};
