qt_standard_project_setup()
qt_add_executable(cesium)
set_target_properties(cesium PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)

# add_executable(cesium)  # WIN32)
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
  # Fix lld-link warning about multiple entry points
  if(WIN32)
    target_link_options(cesium PRIVATE -Xlinker /ENTRY:wWinMainCRTStartup)
  endif()
endif()

target_link_libraries(cesium
  PRIVATE
    ${CMAKE_DL_LIBS}  # For dlopen/dlsym on Unix systems
    Qt6::Core
    Qt6::Widgets
    yyjson
    tree-sitter-core
    tree-sitter-cpp
)

# Deploy Qt libraries after build (for debug/development)
if(WIN32)
  find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS ${QT6_INSTALL_PREFIX}/bin)
  if(WINDEPLOYQT_EXECUTABLE)
    add_custom_command(TARGET cesium POST_BUILD
      COMMAND ${WINDEPLOYQT_EXECUTABLE} --debug --no-translations --no-system-d3d-compiler $<TARGET_FILE:cesium>
      COMMENT "Deploying Qt libraries"
    )
  endif()
endif()

# Installation rules
install(TARGETS cesium
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

add_subdirectory(src)
add_subdirectory(include)
