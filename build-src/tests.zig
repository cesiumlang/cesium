//! The stage 0 test corpus (bootstrap/tests), as build steps.
//!
//!   s0/<name>.csm + <name>.expected   compile with cesium0 (most together with
//!                                     s0/lib.csm), assemble and link with
//!                                     `zig cc`, run, compare stdout
//!   s0/<name>.mir                     golden: the core MIR cesium0 builds
//!   s0/errors/<name>.csm              cesium0 must reject it with the message
//!   mir/<name>.mir + <name>.expected  core MIR text: print it back unchanged,
//!                                     compile it, run, compare stdout
//!   mir/errors/<name>.mir             libcsmir must reject it with the message

const std = @import("std");

const Run = struct {
    name: []const u8,
    expected: []const u8,
    /// The core MIR text to compare with: what cesium0 builds from an S0
    /// test, or the MIR test itself.
    mir: ?[]const u8 = null,
    /// An S0 test compiled together with lib.csm, as one unit.
    lib: bool = false,
};
const Reject = struct { name: []const u8, message: []const u8 };

const s0_dir = "bootstrap/tests/s0/";
const mir_dir = "bootstrap/tests/mir/";

/// An S0 test that uses the output helpers of lib.csm.
fn s0(comptime name: []const u8) Run {
    return .{ .name = name, .expected = @embedFile("../" ++ s0_dir ++ name ++ ".expected"), .lib = true };
}

/// A self-contained S0 test whose core MIR is checked too.
fn s0Golden(comptime name: []const u8) Run {
    return .{
        .name = name,
        .expected = @embedFile("../" ++ s0_dir ++ name ++ ".expected"),
        .mir = @embedFile("../" ++ s0_dir ++ name ++ ".mir"),
    };
}

fn mir(comptime name: []const u8) Run {
    return .{
        .name = name,
        .expected = @embedFile("../" ++ mir_dir ++ name ++ ".expected"),
        .mir = @embedFile("../" ++ mir_dir ++ name ++ ".mir"),
    };
}

const s0_runs = [_]Run{
    s0Golden("tokens"),
    s0("arith"),
    s0("pointers"),
    s0("structs"),
    s0("control"),
    s0("globals"),
};

const s0_rejects = [_]Reject{
    .{ .name = "literal_no_context", .message = "the type of this literal is not determined by its context" },
    .{ .name = "pointer_plain_init", .message = "a pointer is initialized with ':=', '#=' or '~='" },
    .{ .name = "mixed_types", .message = "convert one with 'as'" },
    .{ .name = "plain_to_pointer_param", .message = "expected #i32, found i32" },
    .{ .name = "reserved_word", .message = "is a Cesium keyword that S0 does not have" },
    .{ .name = "unary_minus", .message = "S0 has no unary minus" },
    .{ .name = "shadow", .message = "is already the name of" },
    .{ .name = "hex_literal", .message = "S0 has decimal integer literals only" },
    .{ .name = "alloc_borrow", .message = "alloc returns ownership" },
    .{ .name = "missing_return", .message = "does not return a value on every path" },
};

const mir_runs = [_]Run{
    mir("cstr_len"),
    mir("integers"),
    mir("layout"),
};

const mir_rejects = [_]Reject{
    .{ .name = "type_mismatch", .message = "the operands must have the same type" },
    .{ .name = "break_outside_loop", .message = "break outside a loop" },
    .{ .name = "no_return", .message = "does not return a value on every path" },
    .{ .name = "unknown_local", .message = "unknown local %x" },
    .{ .name = "bad_version", .message = "expected MIR version 0" },
    .{ .name = "struct_by_value_param", .message = "only integers and pointers" },
};

pub fn add(
    b: *std.Build,
    step: *std.Build.Step,
    target: std.Build.ResolvedTarget,
    cesium0: *std.Build.Step.Compile,
    csrt: *std.Build.Step.Compile,
) void {
    for (s0_runs) |t| {
        const source = b.path(b.fmt(s0_dir ++ "{s}.csm", .{t.name}));
        if (t.mir) |golden| {
            const emit = b.addRunArtifact(cesium0);
            emit.addArg("--emit-mir");
            emit.addArg("-o");
            const mir_out = emit.addOutputFileArg(b.fmt("{s}.mir", .{t.name}));
            emit.addFileArg(source);
            step.dependOn(&b.addCheckFile(mir_out, .{ .expected_exact = golden }).step);
        }
        // compile, link, run
        const compile = b.addRunArtifact(cesium0);
        compile.addArg("-o");
        const asm_out = compile.addOutputFileArg(b.fmt("{s}.s", .{t.name}));
        compile.addFileArg(source);
        if (t.lib) compile.addFileArg(b.path(s0_dir ++ "lib.csm"));
        step.dependOn(runProgram(b, target, csrt, b.fmt("s0-{s}", .{t.name}), asm_out, t.expected));
    }
    for (s0_rejects) |t| {
        const run = b.addRunArtifact(cesium0);
        run.addArg("-o");
        _ = run.addOutputFileArg(b.fmt("{s}.s", .{t.name}));
        run.addFileArg(b.path(b.fmt(s0_dir ++ "errors/{s}.csm", .{t.name})));
        run.expectExitCode(1);
        run.addCheck(.{ .expect_stderr_match = t.message });
        step.dependOn(&run.step);
    }
    for (mir_runs) |t| {
        const source = b.path(b.fmt(mir_dir ++ "{s}.mir", .{t.name}));
        // printing what was parsed gives the file back
        const print = b.addRunArtifact(cesium0);
        print.addArg("--emit-mir");
        print.addArg("-o");
        const mir_out = print.addOutputFileArg(b.fmt("{s}.mir", .{t.name}));
        print.addArg("--mir");
        print.addFileArg(source);
        step.dependOn(&b.addCheckFile(mir_out, .{ .expected_exact = t.mir.? }).step);
        // compile, link, run
        const compile = b.addRunArtifact(cesium0);
        compile.addArg("-o");
        const asm_out = compile.addOutputFileArg(b.fmt("{s}.s", .{t.name}));
        compile.addArg("--mir");
        compile.addFileArg(source);
        step.dependOn(runProgram(b, target, csrt, b.fmt("mir-{s}", .{t.name}), asm_out, t.expected));
    }
    for (mir_rejects) |t| {
        const run = b.addRunArtifact(cesium0);
        run.addArg("-o");
        _ = run.addOutputFileArg(b.fmt("{s}.s", .{t.name}));
        run.addArg("--mir");
        run.addFileArg(b.path(b.fmt(mir_dir ++ "errors/{s}.mir", .{t.name})));
        run.expectExitCode(1);
        run.addCheck(.{ .expect_stderr_match = t.message });
        step.dependOn(&run.step);
    }
}

/// Assembles and links what cesium0 wrote, with libcsrt and libc, runs it and
/// compares its output.
fn runProgram(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    csrt: *std.Build.Step.Compile,
    name: []const u8,
    assembly: std.Build.LazyPath,
    expected: []const u8,
) *std.Build.Step {
    const exe = b.addExecutable(.{
        .name = name,
        .root_module = b.createModule(.{ .target = target, .optimize = .Debug, .link_libc = true }),
    });
    exe.root_module.addAssemblyFile(assembly);
    exe.root_module.linkLibrary(csrt);
    const run = b.addRunArtifact(exe);
    // the C library of Windows writes "\r\n" for '\n' to a text stream
    const want = if (target.result.os.tag == .windows)
        std.mem.replaceOwned(u8, b.allocator, expected, "\n", "\r\n") catch @panic("OOM")
    else
        expected;
    run.expectStdOutEqual(want);
    return &run.step;
}
