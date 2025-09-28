add_executable(cesium)
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT cesium)

# Set output directory to build/bin
set_target_properties(cesium PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  # RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
  # RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
)

# Compiler warnings (all platforms)
target_compile_options(cesium PRIVATE -Wall -Wextra -Wpedantic)

# Platform-specific settings
if(WIN32)
  target_compile_definitions(cesium PRIVATE
    _WIN32_WINNT=0x0601
    WIN32_LEAN_AND_MEAN
  )
  # Force console subsystem for MSVC or MinGW, but not for Clang/lld-link
  # if(MSVC OR MINGW)
  #     set_target_properties(cesium PROPERTIES LINK_FLAGS "-Wl,--subsystem,console")
  # endif()
elseif(APPLE)
  # macOS-specific settings
elseif(UNIX)
  # Linux-specific settings
endif()

# Clang-specific flags
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  target_compile_options(cesium PRIVATE -Wno-unused-parameter)
  # # Fix lld-link warning about multiple entry points
  # if(WIN32)
  #   target_link_options(cesium PRIVATE -Xlinker /ENTRY:wWinMainCRTStartup)
  # endif()
endif()

target_link_libraries(cesium
  PRIVATE
    ${CMAKE_DL_LIBS}  # For dlopen/dlsym on Unix systems
    yyjson
    tree-sitter-core
    tree-sitter-cpp
    # utf8
)

# # Copy DLL dependencies to bin directory on Windows
# if(WIN32)
add_custom_command(TARGET cesium POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "$<TARGET_FILE:tree-sitter-cpp>"
    "${CMAKE_BINARY_DIR}/bin/"
  COMMENT "Copying tree-sitter-cpp DLL to bin directory"
)
# endif()

# Installation rules
install(TARGETS cesium
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

add_subdirectory(src)
add_subdirectory(include)
add_subdirectory(gui)
