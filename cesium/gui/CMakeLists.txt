qt_standard_project_setup()
qt_add_executable(cesium-gui gui.cpp)
set_target_properties(cesium-gui PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)
# set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT cesium-gui)

# Set output directory to build/bin
set_target_properties(cesium-gui PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  # RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
  # RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
)

# Compiler warnings (all platforms)
target_compile_options(cesium-gui PRIVATE -Wall -Wextra -Wpedantic)

# Platform-specific settings
if(WIN32)
  target_compile_definitions(cesium-gui PRIVATE
    _WIN32_WINNT=0x0601
    WIN32_LEAN_AND_MEAN
  )
elseif(APPLE)
  # macOS-specific settings
elseif(UNIX)
  # Linux-specific settings
endif()

# Clang-specific flags
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  target_compile_options(cesium-gui PRIVATE -Wno-unused-parameter)
  # Fix lld-link warning about multiple entry points
  if(WIN32)
    target_link_options(cesium-gui PRIVATE -Xlinker /ENTRY:wWinMainCRTStartup)
  endif()
endif()

target_link_libraries(cesium-gui
  PRIVATE
    ${CMAKE_DL_LIBS}  # For dlopen/dlsym on Unix systems
    Qt6::Core
    Qt6::Widgets
)

# Deploy Qt libraries after build (for debug/development)
if(WIN32)
  set(TMP_DEPLOY_TOOL windeployqt)
elseif(APPLE)
  set(TMP_DEPLOY_TOOL macdeployqt)
endif()
set(DEPLOY_TOOL "$<TARGET_NAME_IF_EXISTS:Qt6::${TMP_DEPLOY_TOOL}>")
set(DEPLOY_TOOL_FALLBACK ${TMP_DEPLOY_TOOL})

if(WIN32 OR APPLE)
  add_custom_command(TARGET cesium-gui POST_BUILD
    COMMAND $<IF:$<BOOL:${DEPLOY_TOOL}>,$<TARGET_FILE:${DEPLOY_TOOL}>,${DEPLOY_TOOL_FALLBACK}>
            --debug --no-translations $<TARGET_FILE:cesium-gui>
    COMMENT "Deploying Qt application"
  )
endif()

# Installation rules
install(TARGETS cesium-gui
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

# add_subdirectory(src)
# add_subdirectory(include)
