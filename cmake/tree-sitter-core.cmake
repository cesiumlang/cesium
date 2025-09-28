FetchContent_Declare(
  tree-sitter-core-repo
  GIT_REPOSITORY https://github.com/tree-sitter/tree-sitter.git
  GIT_TAG        v0.25.10
  GIT_SHALLOW    TRUE
  GIT_PROGRESS   TRUE
  SOURCE_SUBDIR  ""  # Prevent any build system from being processed
  CONFIGURE_COMMAND "echo"
)
FetchContent_MakeAvailable(tree-sitter-core-repo)

# Create our own library target from the C source files
add_library(tree-sitter-core STATIC ${tree-sitter-core-repo_SOURCE_DIR}/lib/src/lib.c)
# add_dependencies(tree-sitter-core tree-sitter-core-repo)
target_include_directories(tree-sitter-core
  PRIVATE
    ${tree-sitter-core-repo_SOURCE_DIR}/lib/src
  PUBLIC
    ${tree-sitter-core-repo_SOURCE_DIR}/lib/include
    $<INSTALL_INTERFACE:include>
)

# Set C standard for this target
set_target_properties(tree-sitter-core PROPERTIES
  C_STANDARD 11
  C_STANDARD_REQUIRED TRUE
  CXX_STANDARD 14
  CXX_STANDARD_REQUIRED TRUE
)

target_compile_definitions(tree-sitter-core PRIVATE
  _POSIX_C_SOURCE=200112L
  _DEFAULT_SOURCE
)
