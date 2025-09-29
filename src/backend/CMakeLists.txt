add_library(cesium-backend STATIC)
set_target_properties(cesium-backend PROPERTIES
  OUTPUT_NAME "libcesiumexe"
)

target_include_directories(cesium-backend PUBLIC
  ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(cesium-backend
  PRIVATE
    ${CMAKE_DL_LIBS}  # For dlopen/dlsym on Unix systems
    yyjson
    tree-sitter-core
    tree-sitter-cpp
    utf8
)

# # Copy DLL dependencies to bin directory on Windows
add_custom_command(TARGET cesium-backend POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "$<TARGET_FILE:tree-sitter-cpp>"
    "${CMAKE_BINARY_DIR}/bin/"
  COMMENT "Copying tree-sitter-cpp DLL to bin directory"
)

add_subdirectory(core)
add_subdirectory(doc)
