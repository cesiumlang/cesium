//! The stage 0 test corpus (bootstrap/tests), as build steps.
//!
//!   s0/<name>.csm + <name>.expected   compile with cesium0 (most together with
//!                                     s0/lib.csm), assemble and link with
//!                                     `zig cc`, run, compare stdout
//!   s0/<name>.mir                     golden: the core MIR cesium0 builds,
//!                                     run from the repository root
//!   s0/errors/<name>.csm              cesium0 must reject it with the message
//!   s0/lines.csm, frames.csm, vars.csm  debug information (tests_debug.zig)
//!   mir/<name>.mir + <name>.expected  core MIR text: print it back unchanged,
//!                                     compile it, run, compare stdout
//!   mir/errors/<name>.mir             libcsmir must reject it with the message
//!
//! and cesium1 (bootstrap/stage1, written in S0 and built by cesium0):
//!
//!   for every s0/<name>.csm run test  its core MIR for the same files must
//!                                     be cesium0's --emit-mir output
//!   for every s0/errors/<name>.csm    from standard input, it must reject it
//!                                     with the message
//!   its own source                    its core MIR must be cesium0's, and a
//!                                     second cesium1, built by cesium1 from
//!                                     that source, must give it again; from
//!                                     standard input, the same MIR without
//!                                     its loc lines

const std = @import("std");
const link = @import("link.zig");
const Stage1 = @import("stage1.zig").Stage1;
const tests_debug = @import("tests_debug.zig");

const Run = struct {
    name: []const u8,
    expected: []const u8,
    /// The core MIR text to compare with: what cesium0 builds from an S0
    /// test, or the MIR test itself.
    mir: ?[]const u8 = null,
    /// An S0 test compiled together with lib.csm, as one unit.
    lib: bool = false,
};
/// A test the compiler rejects: its message, and the code it is given
/// (`error[E1101]: `) where the message does not hold it.
const Reject = struct { name: []const u8, message: []const u8, code: ?[]const u8 = null };

fn checkReject(b: *std.Build, run: *std.Build.Step.Run, t: Reject) void {
    run.expectExitCode(1);
    run.addCheck(.{ .expect_stderr_match = t.message });
    if (t.code) |code| run.addCheck(.{ .expect_stderr_match = b.fmt("error[{s}]: ", .{code}) });
}

pub const s0_dir = "bootstrap/tests/s0/";
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
    .{ .name = "missing_return", .code = "E1143", .message = "does not return a value on every path" },
    .{ .name = "deep_expression", .message = "nested too deeply (the limit is 128 levels)" },
    .{ .name = "deep_blocks", .message = "nested too deeply (the limit is 128 levels)" },
    .{ .name = "c_function_header", .message = "[E0214]: expected ';', found '('; a function is written 'T = name(parameters) { ... }'" },
    .{ .name = "arrow_return_type", .message = "[E0215]: expected '{', found '->'; the return type comes first" },
    .{ .name = "c_address_of", .message = "[E0216]: expected an expression, found '&'; an address is taken with '#'" },
    .{ .name = "c_dereference", .message = "[E0217]: expected an expression, found '*'; there is no dereference operator" },
    .{ .name = "compound_assign", .message = "[E0218]: compound assignment is not decided for S0; write x = x + y" },
    .{ .name = "increment", .message = "[E0219]: '++' and '--' are not decided for S0; write x = x + 1" },
    .{ .name = "c_int_type", .message = "[E0320]: unknown type 'int'; the integer types of S0 are u8, i32, u32, i64, u64 and uword" },
    .{ .name = "bool_type", .message = "[E0321]: unknown type 'bool'; there is no bool type: a condition is an integer" },
    .{ .name = "float_type", .message = "[E0322]: unknown type 'double'; S0 has no floating point" },
    .{ .name = "true_false", .message = "[E0323]: unknown name 'true'; true and false are not decided for S0: write 1 or 0" },
};

const mir_runs = [_]Run{
    mir("cstr_len"),
    mir("integers"),
    mir("layout"),
    mir("locs"),
};

const mir_rejects = [_]Reject{
    .{ .name = "type_mismatch", .code = "E1119", .message = "the operands must have the same type" },
    .{ .name = "break_outside_loop", .code = "E1127", .message = "break outside a loop" },
    .{ .name = "no_return", .code = "E1143", .message = "does not return a value on every path" },
    .{ .name = "unknown_local", .code = "E1107", .message = "unknown local %x" },
    .{ .name = "bad_version", .code = "E1020", .message = "expected MIR version 0" },
    .{ .name = "struct_by_value_param", .code = "E1131", .message = "only integers and pointers" },
    .{ .name = "deep_nesting", .code = "E1006", .message = "nested too deeply (the limit is 1024 levels)" },
    .{ .name = "loc_no_file", .code = "E1015", .message = "line 9: the first loc of a function names its file" },
    .{ .name = "loc_line_zero", .code = "E1012", .message = "a line number in a loc is from 1 to 536870911" },
    .{ .name = "loc_column_too_big", .code = "E1012", .message = "a column number in a loc is from 1 to 536870911" },
    .{ .name = "loc_empty_file", .code = "E1013", .message = "empty file name in a loc" },
    .{ .name = "loc_before_brace", .code = "E1016", .message = "a loc stands before a statement" },
    .{ .name = "loc_zero_byte", .code = "E1014", .message = "a file name in a loc has a zero byte" },
};

pub fn add(
    b: *std.Build,
    step: *std.Build.Step,
    target: std.Build.ResolvedTarget,
    cesium0: *std.Build.Step.Compile,
    csrt: *std.Build.Step.Compile,
    csmir: *std.Build.Step.Compile,
    stage1: Stage1,
    tool: *std.Build.Step.Compile,
    devkit: []const u8,
) void {
    for (s0_runs) |t| {
        const source = b.path(b.fmt(s0_dir ++ "{s}.csm", .{t.name}));
        if (t.mir) |golden| {
            const emit = runFromRoot(b, cesium0, t.name);
            emit.addArg("--emit-mir");
            emit.addArg("-o");
            const mir_out = emit.addOutputFileArg(b.fmt("{s}.mir", .{t.name}));
            emit.addArg(b.fmt(s0_dir ++ "{s}.csm", .{t.name}));
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
        checkReject(b, run, t);
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
        checkReject(b, run, t);
        step.dependOn(&run.step);
    }

    tests_debug.add(b, step, target, cesium0, stage1.cesium1);

    addStage1(b, step, target, cesium0, csrt, csmir, stage1, tool, devkit);
}

/// Runs a compiler from the repository root, given the source path as
/// written, so that its locations are the same on every host.
pub fn runFromRoot(b: *std.Build, compiler: *std.Build.Step.Compile, name: []const u8) *std.Build.Step.Run {
    const run = b.addRunArtifact(compiler);
    run.setCwd(b.path("."));
    run.addFileInput(b.path(b.fmt(s0_dir ++ "{s}.csm", .{name})));
    return run;
}

/// cesium1, written in S0 and compiled by cesium0, checked against cesium0.
fn addStage1(
    b: *std.Build,
    step: *std.Build.Step,
    target: std.Build.ResolvedTarget,
    cesium0: *std.Build.Step.Compile,
    csrt: *std.Build.Step.Compile,
    csmir: *std.Build.Step.Compile,
    stage1: Stage1,
    tool: *std.Build.Step.Compile,
    devkit: []const u8,
) void {
    // cesium1 writes its MIR through a C text stream
    const crlf = writesCrlf(target);
    const cesium1 = stage1.cesium1;

    // the S0 run tests, their files in the order cesium0 is given them above
    for (s0_runs) |t| {
        const source = b.path(b.fmt(s0_dir ++ "{s}.csm", .{t.name}));
        const lib = b.path(s0_dir ++ "lib.csm");
        const files: []const std.Build.LazyPath = if (t.lib) &.{ source, lib } else &.{source};
        _ = checkMir(b, step, tool, crlf, cesium0, cesium1, b.fmt("s1-{s}", .{t.name}), files);
    }
    for (s0_rejects) |t| {
        const run = b.addRunArtifact(cesium1);
        run.setName(b.fmt("cesium1 rejects {s}", .{t.name}));
        run.setStdIn(.{ .lazy_path = b.path(b.fmt(s0_dir ++ "errors/{s}.csm", .{t.name})) });
        run.expectExitCode(1);
        run.addCheck(.{ .expect_stdout_match = t.message });
        step.dependOn(&run.step);
    }

    // the fixed point: cesium1 compiles itself to cesium0's MIR, and the
    // cesium1 it builds from its own source does the same
    const self = checkMir(b, step, tool, crlf, cesium0, cesium1, "s1-self", stage1.sources);
    const compile2 = b.addRunArtifact(cesium1);
    compile2.setName("cesium1 compiles cesium1");
    compile2.addArg("-o");
    const cesium2_asm = compile2.addOutputFileArg("cesium1-gen2.s");
    for (stage1.sources) |source| compile2.addFileArg(source);
    compile2.expectExitCode(0);
    const cesium2 = link.linkCompiler(b, target, csrt, csmir, devkit, "cesium1-gen2", cesium2_asm);
    _ = checkMirWith(b, step, tool, crlf, cesium2, "s1-gen2-self", stage1.sources, self);

    // from standard input: the same MIR without the locations
    const cat = b.addRunArtifact(tool);
    cat.addArg("cat");
    const unit = cat.addOutputFileArg("s1-self-stdin.csm");
    for (stage1.sources) |source| cat.addFileArg(source);
    checkMirOf(b, step, tool, crlf, cesium1, "s1-self-stdin", unit, self);
}

/// build-src/stage1_tool.c, for the host: it joins and compares files.
pub fn addTool(b: *std.Build) *std.Build.Step.Compile {
    const tool = b.addExecutable(.{
        .name = "stage1_tool",
        .root_module = b.createModule(.{ .target = b.graph.host, .optimize = .Debug, .link_libc = true }),
    });
    tool.root_module.addCSourceFile(.{
        .file = b.path("build-src/stage1_tool.c"),
        .flags = &.{ "-std=c99", "-Wall", "-Wextra", "-Werror" },
    });
    return tool;
}

/// Whether cesium1's output has CRLF line endings: it writes through a C
/// text stream.
pub fn writesCrlf(target: std.Build.ResolvedTarget) bool {
    return target.result.os.tag == .windows;
}

/// Checks that cesium1 gives cesium0's core MIR for the files, named on the
/// command line of each as one unit. Returns cesium0's MIR.
fn checkMir(
    b: *std.Build,
    step: *std.Build.Step,
    tool: *std.Build.Step.Compile,
    crlf: bool,
    cesium0: *std.Build.Step.Compile,
    cesium1: *std.Build.Step.Compile,
    name: []const u8,
    files: []const std.Build.LazyPath,
) std.Build.LazyPath {
    const emit = b.addRunArtifact(cesium0);
    emit.addArg("--emit-mir");
    emit.addArg("-o");
    const reference = emit.addOutputFileArg(b.fmt("{s}.mir", .{name}));
    for (files) |file| emit.addFileArg(file);
    return checkMirWith(b, step, tool, crlf, cesium1, name, files, reference);
}

/// Runs a cesium1 on the files and compares its output with the reference
/// MIR. Returns the reference.
fn checkMirWith(
    b: *std.Build,
    step: *std.Build.Step,
    tool: *std.Build.Step.Compile,
    crlf: bool,
    cesium1: *std.Build.Step.Compile,
    name: []const u8,
    files: []const std.Build.LazyPath,
    reference: std.Build.LazyPath,
) std.Build.LazyPath {
    const run = b.addRunArtifact(cesium1);
    run.setName(b.fmt("{s} {s}", .{ cesium1.name, name }));
    for (files) |file| run.addFileArg(file);
    run.expectExitCode(0);
    compare(b, step, tool, crlf, false, name, reference, run.captureStdOut(.{ .basename = b.fmt("{s}.out.mir", .{name}) }));
    return reference;
}

/// Runs a cesium1 on a unit given on standard input, where it has no file
/// names, and compares its output with the reference MIR without its loc
/// lines.
pub fn checkMirOf(
    b: *std.Build,
    step: *std.Build.Step,
    tool: *std.Build.Step.Compile,
    crlf: bool,
    cesium1: *std.Build.Step.Compile,
    name: []const u8,
    unit: std.Build.LazyPath,
    reference: std.Build.LazyPath,
) void {
    const run = b.addRunArtifact(cesium1);
    run.setName(b.fmt("{s} {s}", .{ cesium1.name, name }));
    run.setStdIn(.{ .lazy_path = unit });
    run.expectExitCode(0);
    compare(b, step, tool, crlf, true, name, reference, run.captureStdOut(.{ .basename = b.fmt("{s}.out.mir", .{name}) }));
}

fn compare(
    b: *std.Build,
    step: *std.Build.Step,
    tool: *std.Build.Step.Compile,
    crlf: bool,
    noloc: bool,
    name: []const u8,
    reference: std.Build.LazyPath,
    output: std.Build.LazyPath,
) void {
    const same = b.addRunArtifact(tool);
    same.setName(b.fmt("compare {s} with cesium0", .{name}));
    same.addArg("same");
    same.addFileArg(reference);
    same.addFileArg(output);
    if (crlf) same.addArg("crlf");
    if (noloc) same.addArg("noloc");
    same.expectExitCode(0);
    step.dependOn(&same.step);
}

/// Expected output as the target's C library writes it: on Windows a text
/// stream gets "\r\n" for every '\n'.
fn hostText(b: *std.Build, target: std.Build.ResolvedTarget, text: []const u8) []const u8 {
    if (target.result.os.tag != .windows) return text;
    return std.mem.replaceOwned(u8, b.allocator, text, "\n", "\r\n") catch @panic("OOM");
}

/// Links what cesium0 wrote, runs it and compares its output.
fn runProgram(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    csrt: *std.Build.Step.Compile,
    name: []const u8,
    assembly: std.Build.LazyPath,
    expected: []const u8,
) *std.Build.Step {
    const run = b.addRunArtifact(link.linkProgram(b, target, csrt, name, assembly));
    run.expectStdOutEqual(hostText(b, target, expected));
    return &run.step;
}
