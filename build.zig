const std = @import("std");

fn createYyjsonLib(b: *std.Build, target: std.Build.ResolvedTarget, optimize: std.builtin.OptimizeMode) *std.Build.Step.Compile {
    const yyjson_dep = b.dependency("yyjson", .{});

    const lib = b.addLibrary(.{
        .name = "yyjson",
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
        }),
    });

    lib.addCSourceFiles(.{
        .root = yyjson_dep.path(""),
        .files = &[_][]const u8{
            "src/yyjson.c",
        },
        .flags = &[_][]const u8{
            "-std=c99",
        },
    });

    lib.addIncludePath(yyjson_dep.path("src"));
    lib.linkLibC();

    return lib;
}

fn createTreeSitterCppLib(b: *std.Build, target: std.Build.ResolvedTarget, optimize: std.builtin.OptimizeMode) *std.Build.Step.Compile {
    const tree_sitter_cpp_dep = b.dependency("tree_sitter_cpp", .{});

    const lib = b.addLibrary(.{
        .name = "tree-sitter-cpp",
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
        }),
    });

    lib.addCSourceFiles(.{
        .root = tree_sitter_cpp_dep.path(""),
        .files = &[_][]const u8{
            "src/parser.c",
            "src/scanner.c",
        },
        .flags = &[_][]const u8{
            "-std=c11", // Use C11 for static_assert support
        },
    });

    lib.linkLibC();

    return lib;
}

fn createTreeSitterCoreLib(b: *std.Build, target: std.Build.ResolvedTarget, optimize: std.builtin.OptimizeMode) *std.Build.Step.Compile {
    const lib = b.addLibrary(.{
        .name = "tree-sitter-core",
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
        }),
    });

    // Add tree-sitter core from git submodule
    lib.addCSourceFiles(.{
        .root = b.path("."),
        .files = &[_][]const u8{
            "thirdparty/tree-sitter/lib/src/lib.c", // Amalgamated source
        },
        .flags = &[_][]const u8{
            "-std=c11",
        },
    });

    // Add tree-sitter include paths
    lib.addIncludePath(b.path("thirdparty/tree-sitter/lib/include"));
    lib.addIncludePath(b.path("thirdparty/tree-sitter/lib/src"));

    // Add tree-sitter macros
    lib.root_module.addCMacro("_POSIX_C_SOURCE", "200112L");
    lib.root_module.addCMacro("_DEFAULT_SOURCE", "");

    lib.linkLibC();

    return lib;
}

fn createCesiumExe(b: *std.Build, target: std.Build.ResolvedTarget, optimize: std.builtin.OptimizeMode) *std.Build.Step.Compile {
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
            "-std=c++20", // CMAKE_CXX_STANDARD 20
            "-Wall", // target_compile_options
            "-Wextra",
            "-Wpedantic",
            "-Wno-unused-parameter", // Clang-specific flag from CMake
        },
    });

    // Link libraries
    exe.linkLibCpp();
    exe.linkLibC();

    return exe;
}

fn setupBuildSteps(b: *std.Build, exe: *std.Build.Step.Compile) void {
    // Install the executable
    // b.installArtifact(exe);
    const install = b.addInstallArtifact(exe, .{
        .dest_dir = .{ .override = .{ .custom = "../build/bin" } },
        .pdb_dir = .{ .override = .{ .custom = "../build/bin" } },
        .h_dir = .{ .override = .{ .custom = "../build/include" } },
    });
    b.default_step.dependOn(&install.step);

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

    const exe = createCesiumExe(b, target, optimize);

    const yyjson_lib = createYyjsonLib(b, target, optimize);
    exe.linkLibrary(yyjson_lib);
    exe.addIncludePath(b.dependency("yyjson", .{}).path("src"));

    const tree_sitter_core_lib = createTreeSitterCoreLib(b, target, optimize);
    exe.linkLibrary(tree_sitter_core_lib);
    exe.addIncludePath(b.path("thirdparty/tree-sitter/lib/include"));

    const tree_sitter_cpp_lib = createTreeSitterCppLib(b, target, optimize);
    exe.linkLibrary(tree_sitter_cpp_lib);

    setupBuildSteps(b, exe);
}
