const std = @import("std");

fn setupYyJsonDependency(b: *std.Build, exe: *std.Build.Step.Compile) void {
    const yyjson_dep = b.dependency("yyjson", .{});

    exe.addCSourceFiles(.{
        .root = yyjson_dep.path(""),
        .files = &[_][]const u8{
            "src/yyjson.c",
        },
        .flags = &[_][]const u8{
            "-std=c99",
        },
    });

    exe.addIncludePath(yyjson_dep.path("src"));
}

fn setupTreeSitterCppDependency(b: *std.Build, exe: *std.Build.Step.Compile) void {
    const tree_sitter_cpp_dep = b.dependency("tree_sitter_cpp", .{});

    exe.addCSourceFiles(.{
        .root = tree_sitter_cpp_dep.path(""),
        .files = &[_][]const u8{
            "src/parser.c",
            "src/scanner.c", 
        },
        .flags = &[_][]const u8{
            "-std=c11",  // Use C11 for static_assert support
        },
    });
}

fn setupTreeSitterCoreDependency(b: *std.Build, exe: *std.Build.Step.Compile) void {
    // Add tree-sitter core from git submodule
    exe.addCSourceFiles(.{
        .root = b.path("."),
        .files = &[_][]const u8{
            "thirdparty/tree-sitter/lib/src/lib.c",  // Amalgamated source
        },
        .flags = &[_][]const u8{
            "-std=c11",
        },
    });
    
    // Add tree-sitter include paths
    exe.addIncludePath(b.path("thirdparty/tree-sitter/lib/include"));
    exe.addIncludePath(b.path("thirdparty/tree-sitter/lib/src"));
    
    // Add tree-sitter macros
    exe.root_module.addCMacro("_POSIX_C_SOURCE", "200112L");
    exe.root_module.addCMacro("_DEFAULT_SOURCE", "");
}

fn configureExecutable(b: *std.Build, target: std.Build.ResolvedTarget, optimize: std.builtin.OptimizeMode) *std.Build.Step.Compile {
    const exe = b.addExecutable(.{
        .name = "cesium",
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
        }),
    });

    // Add C++ source files
    exe.addCSourceFiles(.{
        .files = &[_][]const u8{
            "cesium/src/main.cpp",
        },
        .flags = &[_][]const u8{
            "-std=c++20",  // CMAKE_CXX_STANDARD 20
            "-Wall",       // target_compile_options
            "-Wextra",
            "-Wpedantic", 
            "-Wno-unused-parameter",  // Clang-specific flag from CMake
        },
    });

    // Link libraries
    exe.linkLibCpp();
    exe.linkLibC();

    return exe;
}

fn setupBuildSteps(b: *std.Build, exe: *std.Build.Step.Compile) void {
    // Install the executable
    b.installArtifact(exe);

    // Create a run step
    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());

    // Allow passing arguments to the application
    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

    // Create a run step that can be executed with `zig build run`
    const run_step = b.step("run", "Run the cesium compiler");
    run_step.dependOn(&run_cmd.step);

    // Create test step for future use
    const test_step = b.step("test", "Run unit tests");
    _ = test_step; // Suppress unused variable warning for now
}

pub fn build(b: *std.Build) void {
    // Standard target and optimization options
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    // Configure the main executable
    const exe = configureExecutable(b, target, optimize);

    // Setup all dependencies
    setupYyJsonDependency(b, exe);
    setupTreeSitterCppDependency(b, exe);
    setupTreeSitterCoreDependency(b, exe);

    // Setup build steps
    setupBuildSteps(b, exe);
}