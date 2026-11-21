//! The step fuzz: tools/fuzz-stage0, mutation fuzzing of cesium0, run at the
//! root of the repository with the arguments after `--`.
//!
//!   zig build fuzz -- [<cesium0>] [--iterations N] [--seed S] [--out DIR]
//!                     [--timeout SECONDS] [--jobs N]
//!
//! Without <cesium0>, it fuzzes the cesium0 of this build, which it passes as
//! --default-cesium0; tools/portable-c.sh fuzz passes one built with
//! sanitizers.

const std = @import("std");

pub fn add(b: *std.Build, step: *std.Build.Step, cesium0: *std.Build.Step.Compile) void {
    const fuzzer = b.addExecutable(.{
        .name = "fuzz-stage0",
        .root_module = b.createModule(.{
            .root_source_file = b.path("tools/fuzz-stage0/main.zig"),
            .target = b.graph.host,
            .optimize = .ReleaseSafe,
        }),
    });
    const run = b.addRunArtifact(fuzzer);
    run.setName("fuzz cesium0");
    run.setCwd(b.path("."));
    run.has_side_effects = true;
    run.addPrefixedArtifactArg("--default-cesium0=", cesium0);
    run.addPassthruArgs();
    step.dependOn(&run.step);
}
