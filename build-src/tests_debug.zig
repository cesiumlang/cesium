//! The tests of debug information, as build steps, and the programs the
//! debuggers of CI run.
//!
//!   s0/lines.csm   the assembly of both compilers must have its file and the
//!                  line of a trap, with a DWARF compile unit for ELF and
//!                  Mach-O or CodeView for COFF (the host's, and cesium0's
//!                  for x86_64-linux, aarch64-macos and x86_64-windows)
//!   s0/frames.csm  COFF: the unwind data of each prologue
//!   s0/vars.csm    the variables and scopes of a debug build, for each format

const std = @import("std");
const link = @import("link.zig");
const tests = @import("tests.zig");

const s0_dir = tests.s0_dir;
const runFromRoot = tests.runFromRoot;

/// The checks of the assembly: lines from both compilers, unwind data and
/// variables from cesium0 for each format.
pub fn add(
    b: *std.Build,
    step: *std.Build.Step,
    target: std.Build.ResolvedTarget,
    cesium0: *std.Build.Step.Compile,
    cesium1: *std.Build.Step.Compile,
) void {
    // line information: both compilers' assembly names the file and the line,
    // with a DWARF compile unit, or CodeView for COFF
    for ([_]*std.Build.Step.Compile{ cesium0, cesium1 }) |compiler| {
        const compile = runFromRoot(b, compiler, lines.name);
        compile.addArg("-o");
        const asm_out = compile.addOutputFileArg(b.fmt("{s}-{s}.s", .{ lines.name, compiler.name }));
        compile.addArg(s0_dir ++ lines.name ++ ".csm");
        compile.expectExitCode(0);
        step.dependOn(&b.addCheckFile(asm_out, .{ .expected_matches = lines.forFormat(target.result.ofmt) }).step);
    }
    // and for an ELF, a Mach-O and a COFF target on every host
    for (lines.cross) |cross| {
        const compile = runFromRoot(b, cesium0, lines.name);
        compile.addArgs(&.{ "--target", cross.triple, "-o" });
        const asm_out = compile.addOutputFileArg(b.fmt("{s}-{s}.s", .{ lines.name, cross.triple }));
        compile.addArg(s0_dir ++ lines.name ++ ".csm");
        compile.expectExitCode(0);
        step.dependOn(&b.addCheckFile(asm_out, .{ .expected_matches = lines.forFormat(cross.ofmt) }).step);
    }
    // the unwind data of a COFF target, on every host
    {
        const compile = runFromRoot(b, cesium0, frames.name);
        compile.addArgs(&.{ "--target", "x86_64-windows", "-o" });
        const asm_out = compile.addOutputFileArg(frames.name ++ "-x86_64-windows.s");
        compile.addArg(s0_dir ++ frames.name ++ ".csm");
        compile.expectExitCode(0);
        step.dependOn(&b.addCheckFile(asm_out, .{ .expected_matches = &frames.coff }).step);
    }
    // and variables, in the default mode, debug, for each format
    for (vars.cross) |cross| {
        const compile = runFromRoot(b, cesium0, vars.name);
        compile.addArgs(&.{ "--target", cross.triple, "-o" });
        const asm_out = compile.addOutputFileArg(b.fmt("{s}-{s}.s", .{ vars.name, cross.triple }));
        compile.addArg(s0_dir ++ vars.name ++ ".csm");
        compile.expectExitCode(0);
        step.dependOn(&b.addCheckFile(asm_out, .{ .expected_matches = cross.matches }).step);
    }
}

/// s0/lines.csm divides by zero at a known place; its assembly must carry
/// that line.
const lines = struct {
    const name = "lines";
    const matches = [_][]const u8{
        ".file 1 \"" ++ s0_dir ++ name ++ ".csm\"",
        // no line ending: QBE writes a text stream, "\r\n" on Windows
        "\t.loc 1 12 3",
    };
    /// The compile unit libcsmir adds (bootstrap/libcsmir/mir_dwarf.c).
    const unit = [_][]const u8{
        "\t.short 4",
        "\t.asciz \"libcsmir\"",
        "\t.asciz \"" ++ s0_dir ++ name ++ ".csm\"",
        "\t.set ",
    };
    const elf = matches ++ unit ++ [_][]const u8{
        ".Lcs_text_start:",
        ".Lcs_text_end:",
        ".section .debug_info,\"\",@progbits",
        "\t.long .Lcs_line",
        "\t.quad .Lcs_text_start",
    };
    const macho = matches ++ unit ++ [_][]const u8{
        "\nLcs_text_start:",
        "\nLcs_text_end:",
        ".section __DWARF,__debug_info,regular,debug",
        ".section __DWARF,__debug_str,regular,debug",
        "\t.quad Lcs_text_start",
    };
    /// CodeView instead (bootstrap/libcsmir/mir_codeview.c): the path absolute,
    /// with backslashes, escaped as an assembler string.
    const coff = [_][]const u8{
        ".cv_file 1 \"",
        "\\\\bootstrap\\\\tests\\\\s0\\\\" ++ name ++ ".csm\"",
        // the entry has the line of the first statement, for stepping in
        "main:\n\t.cv_func_id 0\n\t.cv_loc 0 1 7 3\n.seh_proc main\n\tendbr64",
        "\t.cv_loc 0 1 12 3",
        // unwind data (bootstrap/libcsmir/mir_seh.c)
        "\t.seh_endprologue",
        "\t.seh_endproc\n.Lcs_cv_end0:",
        ".section .debug$S,\"dr\"",
        "\t.short 0x1110",
        "\t.secrel32 main",
        "\t.cv_linetable 0, main, .Lcs_cv_end0",
        "\t.cv_filechecksums",
    };
    const cross = [_]struct { triple: []const u8, ofmt: std.Target.ObjectFormat }{
        .{ .triple = "x86_64-linux", .ofmt = .elf },
        .{ .triple = "aarch64-macos", .ofmt = .macho },
        .{ .triple = "x86_64-windows", .ofmt = .coff },
    };

    /// What the assembly for objects of the format must have.
    fn forFormat(ofmt: std.Target.ObjectFormat) []const []const u8 {
        return switch (ofmt) {
            .elf => &elf,
            .macho => &macho,
            else => &coff,
        };
    }
};

/// s0/frames.csm traps two calls deep, below a frame too large for an unwind
/// record's frame offset; its COFF assembly must describe each prologue
/// (bootstrap/libcsmir/mir_seh.c).
const frames = struct {
    const name = "frames";
    const coff = [_][]const u8{
        ".seh_proc leaf",
        // the frame set at once, as it is too large to be set after the
        // allocation
        ".seh_proc wide\n\tendbr64\n\tpushq %rbp\n\t.seh_pushreg %rbp\n\tmovq %rsp, %rbp\n" ++
            "\t.seh_setframe %rbp, 0\n\t.seh_endprologue\n\tsubq $528, %rsp",
        ".seh_proc main",
        "\t.seh_endproc\n.Lcs_cv_end2:",
    };
};

/// s0/vars.csm has locals in nested scopes; in a debug build its assembly
/// keeps them in memory and describes them (mir_debug.c,
/// mir_codeview_vars.c, mir_dwarf_vars.c).
const vars = struct {
    const name = "vars";
    const coff = [_][]const u8{
        // each slot's address escapes to its marker
        "\tleaq 28(%rsp), %rax\n\tmovq %rax, .Lcs_v0_0(%rip)",
        // the if block of scale is a scope
        ".Lcs_sb0_1:\n\tmovb $0, .Lcs_o0_1(%rip)",
        ".Lcs_se0_1:",
        // the trap of the division, out of the scope, has a block of its own
        ".Lcs_tb0_0:\n\tmovb $0, .Lcs_t0_1(%rip)\n\tud2\n.Lcs_te0_0:",
        "\t.long .Lcs_te0_0 - .Lcs_tb0_0",
        // S_REGREL32 at rsp + 28, a pointer to Pair, and S_BLOCK32
        "\t.short 0x1111\n\t.long 28\n\t.long 0x1001\n\t.short 335\n\t.asciz \"p\"",
        "\t.asciz \"inner\"",
        "\t.short 0x1103",
        ".section .debug$T,\"dr\"",
    };
    /// DWARF instead (mir_dwarf_vars.c): the function's and the scope's
    /// labels, and the unit's children
    const elf = [_][]const u8{
        "scale:\n.Lcs_fb0:",
        ".Lcs_sb0_1:\n\tmovb $0, .Lcs_o0_1(%rip)",
        ".Lcs_se0_1:",
        ".Lcs_tb0_0:\n\tmovb $0, .Lcs_t0_1(%rip)\n\tud2\n.Lcs_te0_0:",
        "\t.quad .Lcs_tb0_0\n\t.set .Lcs_d2, .Lcs_te0_0 - .Lcs_tb0_0",
        ".Lcs_fe0:\n/* end function scale */",
        "\t.asciz \"inner\"",
        "\t.quad .Lcs_fb0\n\t.set .Lcs_d0, .Lcs_fe0 - .Lcs_fb0",
        // p at rsp + 20: DW_OP_breg7 20
        "\t.byte 2, 0x77, 0x14",
    };
    const macho = [_][]const u8{
        "_scale:\nLcs_fb0:",
        "Lcs_sb0_1:\n\tadrp",
        "Lcs_tb0_0:\n\tadrp\tx1, _.Lcs_t0_1@page\n\tadd\tx1, x1, _.Lcs_t0_1@pageoff\n",
        "\tbrk\t#1000\nLcs_te0_0:",
        // a function's size in the debug map is right only with it
        ".subsections_via_symbols\n",
        "\t.asciz \"inner\"",
        "\t.set Lcs_d0, Lcs_fe0 - Lcs_fb0",
        // on AArch64, locals are at offsets from x29: DW_OP_breg29
        "\t.byte 2, 0x8d,",
    };
    const cross = [_]struct { triple: []const u8, matches: []const []const u8 }{
        .{ .triple = "x86_64-windows", .matches = &coff },
        .{ .triple = "x86_64-linux", .matches = &elf },
        .{ .triple = "aarch64-macos", .matches = &macho },
    };
};

/// s0/lines.csm, s0/frames.csm and s0/vars.csm as programs, compiled as the
/// tests compile them in the build's mode, for the debuggers to run (the gdb,
/// lldb and cdb steps of CI).
pub fn addLines(
    b: *std.Build,
    step: *std.Build.Step,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    cesium0: *std.Build.Step.Compile,
    csrt: *std.Build.Step.Compile,
) void {
    for ([_][]const u8{ lines.name, frames.name, vars.name }) |name| {
        const compile = runFromRoot(b, cesium0, name);
        compile.addArgs(&.{ "-O", @tagName(optimize), "-o" });
        const asm_out = compile.addOutputFileArg(b.fmt("{s}.s", .{name}));
        compile.addArg(b.fmt(s0_dir ++ "{s}.csm", .{name}));
        const exe = link.linkProgram(b, target, csrt, name, asm_out);
        step.dependOn(&b.addInstallArtifact(exe, .{}).step);
    }
}
