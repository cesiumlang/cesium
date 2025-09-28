FetchContent_Declare(
  utf8
  # GIT_REPOSITORY https://github.com/neacsum/utf8.git
  # GIT_TAG fb3cc08e7244c835b4ae7854dd3716c8b0d7c394
  GIT_REPOSITORY https://github.com/cesiumlang/utf8.git
  GIT_TAG fix-clang
  GIT_PROGRESS TRUE
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(utf8)
