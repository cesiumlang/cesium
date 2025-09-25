FetchContent_Declare(
  tree-sitter-cpp-repo
  GIT_REPOSITORY https://github.com/tree-sitter/tree-sitter-cpp.git
  GIT_TAG v0.23.4
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE
  SOURCE_SUBDIR "randy"
)
FetchContent_MakeAvailable(tree-sitter-cpp-repo)

# Create our own library target without the CLI generation step
add_library(tree-sitter-cpp SHARED
  ${tree-sitter-cpp-repo_SOURCE_DIR}/src/parser.c
  ${tree-sitter-cpp-repo_SOURCE_DIR}/src/scanner.c
)
target_include_directories(tree-sitter-cpp PUBLIC
  ${tree-sitter-cpp-repo_SOURCE_DIR}/src
)
target_link_libraries(tree-sitter-cpp PUBLIC tree-sitter-core)
