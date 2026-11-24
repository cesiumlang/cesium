//! Links what cesium0 or cesium1 wrote: shared by cesium1's build and the tests.

const std = @import("std");

/// Links a compiler from assembly: it calls libcsmir, which runs QBE in-process.
pub fn linkCompiler(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    csrt: *std.Build.Step.Compile,
    csmir: *std.Build.Step.Compile,
    devkit: []const u8,
    name: []const u8,
    assembly: std.Build.LazyPath,
) *std.Build.Step.Compile {
    const exe = linkProgram(b, target, csrt, name, assembly);
    exe.root_module.linkLibrary(csmir);
    // QBE, as cesium0 links it (build-src/__root__.zig)
    exe.root_module.addObjectFile(.{ .cwd_relative = b.pathJoin(&.{ devkit, "qbe", "lib", "libqbe.a" }) });
    return exe;
}

/// Assembles and links a program, with libcsrt and libc.
pub fn linkProgram(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    csrt: *std.Build.Step.Compile,
    name: []const u8,
    assembly: std.Build.LazyPath,
) *std.Build.Step.Compile {
    const exe = b.addExecutable(.{
        .name = name,
        .root_module = b.createModule(.{ .target = target, .optimize = .Debug, .link_libc = true }),
    });
    exe.root_module.addAssemblyFile(assembly);
    exe.root_module.linkLibrary(csrt);
    return exe;
}
