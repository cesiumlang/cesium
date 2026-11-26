//! Builds stage 0 of the Cesium bootstrap chain (cesiumlang/cesium#9):
//!
//!   cesium0    the stage 0 compiler: S0 source or core MIR text -> assembly
//!   libcsmir   core MIR: text parser and printer, verifier, QBE lowering
//!   libcsrt    the runtime behind S0's alloc(count)
//!
//! Everything is portable C (cesiumlang/cesium#10) compiled by `zig cc`. QBE
//! comes from the devkit (cesiumlang/devkits) as libqbe.a; pass its
//! directory with -Ddevkit=<dir> (default: .devkit). With them it builds
//! stage 1:
//!
//!   cesium1    the S0 compiler written in S0 (bootstrap/stage1), compiled
//!              by cesium0
//!
//!   zig build         build and install to zig-out
//!   zig build test       build, then run the test corpus and check the
//!                        diagnostic codes (diagnostics.zig)
//!   zig build test-diff  cesium1 against cesium0 on the generated corpus
//!   zig build fuzz       cesium0 on damaged copies of the corpus (fuzz.zig)

const std = @import("std");
const stage1 = @import("stage1.zig");
const tests = @import("tests.zig");
const tests_diff = @import("tests_diff.zig");
const fuzz = @import("fuzz.zig");
const diagnostics = @import("diagnostics.zig");

/// The flags of the stage 0 C dialect (cesiumlang/cesium#10, "Flags"), minus
/// the language standard, which the callers add.
const c_flags = [_][]const u8{
    "-pedantic-errors",
    "-Wall",
    "-Wextra",
    "-Wvla",
    "-Wconversion",
    "-Wformat=2",
    "-Wformat-nonliteral",
    // -Wformat=2 asks for format attributes, which the dialect bans
    "-Wno-missing-format-attribute",
    "-Wimplicit-fallthrough",
    "-Wstrict-prototypes",
    "-Wmissing-prototypes",
    "-Werror",
};

const csmir_sources = [_][]const u8{
    "cs_common.c",
    "mir.c",
    "mir_text.c",
    "mir_print.c",
    "mir_verify.c",
    "mir_qbe.c",
    "mir_dwarf.c",
    "csmir_api.c",
};

const stage0_sources = [_][]const u8{
    "main.c",
    "s0_lex.c",
    "s0_parse.c",
    "s0_lower.c",
    "s0_habit.c",
};

const Stage0 = struct {
    cesium0: *std.Build.Step.Compile,
    csmir: *std.Build.Step.Compile,
    csrt: *std.Build.Step.Compile,
};

pub fn build(b: *std.Build) tests_diff.Error!void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const devkit = b.option([]const u8, "devkit", "The unpacked Cesium devkit (default: .devkit)") orelse ".devkit";

    // the dialect is the intersection of C99 and C11: build as both
    const c99 = addStage0(b, target, optimize, devkit, "-std=c99", "");
    const c11 = addStage0(b, target, optimize, devkit, "-std=c11", "-c11");

    b.installArtifact(c99.cesium0);
    b.installArtifact(c99.csmir);
    b.installArtifact(c99.csrt);
    // the cesium1 the tests check
    const s1 = stage1.add(b, target, c99.cesium0, c99.csrt, c99.csmir, devkit);
    b.installArtifact(s1.cesium1);

    const tool = tests.addTool(b);
    const test_step = b.step("test", "Run the stage 0 test corpus");
    test_step.dependOn(&c11.cesium0.step);
    tests.add(b, test_step, target, c99.cesium0, c99.csrt, c99.csmir, s1, tool, devkit);
    diagnostics.add(b, test_step, &stage0_sources, &s1.sources);

    const lines_step = b.step("lines", "Build bootstrap/tests/s0/lines.csm into zig-out/bin, for a debugger");
    tests.addLines(b, lines_step, target, c99.cesium0, c99.csrt);

    const diff_step = b.step("test-diff", "Compare cesium1 with cesium0 on the generated corpus");
    try tests_diff.add(b, diff_step, target, tool, c99.cesium0, s1.cesium1);

    const fuzz_step = b.step("fuzz", "Fuzz cesium0 with damaged copies of the test corpus");
    fuzz.add(b, fuzz_step, c99.cesium0);
}

fn addStage0(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    devkit: []const u8,
    comptime std_flag: []const u8,
    comptime suffix: []const u8,
) Stage0 {
    const flags = [_][]const u8{std_flag} ++ c_flags;
    // the one file that forwards format strings (bootstrap/include/cs_common.h)
    const format_flags = flags ++ [_][]const u8{"-Wno-format-nonliteral"};

    const csmir = b.addLibrary(.{
        .name = "csmir" ++ suffix,
        .linkage = .static,
        .root_module = b.createModule(.{ .target = target, .optimize = optimize, .link_libc = true }),
    });
    csmir.root_module.addIncludePath(b.path("bootstrap/include"));
    csmir.root_module.addCSourceFiles(.{ .root = b.path("bootstrap/libcsmir"), .files = &csmir_sources, .flags = &flags });
    csmir.root_module.addCSourceFiles(.{ .root = b.path("bootstrap/libcsmir"), .files = &.{"cs_format.c"}, .flags = &format_flags });

    const csrt = b.addLibrary(.{
        .name = "csrt" ++ suffix,
        .linkage = .static,
        .root_module = b.createModule(.{ .target = target, .optimize = optimize, .link_libc = true }),
    });
    csrt.root_module.addIncludePath(b.path("bootstrap/include"));
    csrt.root_module.addCSourceFiles(.{ .root = b.path("bootstrap/runtime"), .files = &.{"cs_arena.c"}, .flags = &flags });
    csrt.root_module.addCSourceFiles(.{ .root = b.path("bootstrap/libcsmir"), .files = &.{"cs_common.c"}, .flags = &flags });

    const cesium0 = b.addExecutable(.{
        .name = "cesium0" ++ suffix,
        .root_module = b.createModule(.{ .target = target, .optimize = optimize, .link_libc = true }),
    });
    cesium0.root_module.addIncludePath(b.path("bootstrap/include"));
    cesium0.root_module.addIncludePath(b.path("bootstrap/libcsmir"));
    cesium0.root_module.addCSourceFiles(.{ .root = b.path("bootstrap/stage0"), .files = &stage0_sources, .flags = &flags });
    cesium0.root_module.linkLibrary(csmir);
    // QBE, third-party code built by the devkit pipeline with its entry point
    // renamed to qbe_main
    cesium0.root_module.addObjectFile(.{ .cwd_relative = b.pathJoin(&.{ devkit, "qbe", "lib", "libqbe.a" }) });

    return .{ .cesium0 = cesium0, .csmir = csmir, .csrt = csrt };
}
