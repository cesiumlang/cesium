include_guard(GLOBAL)

macro(ExeTgtDefaults tgt)
  # add_executable(${tgt})

  # Set output directory to build/bin
  set_target_properties(${tgt} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    # RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    # RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
  )

  # Compiler warnings (all platforms)
  target_compile_options(${tgt} PRIVATE -Wall -Wextra -Wpedantic)

  # Platform-specific settings
  if(WIN32)
    target_compile_definitions(${tgt} PRIVATE
      _WIN32_WINNT=0x0601
      WIN32_LEAN_AND_MEAN
      NOMINMAX  # https://learn.microsoft.com/en-us/archive/msdn-magazine/2016/september/c-unicode-encoding-conversions-with-stl-strings-and-win32-apis#converting-from-utf-8-to-utf-16-multibytetowidechar-in-action
    )
    # Force console subsystem for MSVC or MinGW, but not for Clang/lld-link
    # if(MSVC OR MINGW)
    #     set_target_properties(${tgt} PROPERTIES LINK_FLAGS "-Wl,--subsystem,console")
    # endif()
  elseif(APPLE)
    # macOS-specific settings
  elseif(UNIX)
    # Linux-specific settings
  endif()

  # Clang-specific flags
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(${tgt} PRIVATE -Wno-unused-parameter)
    # # Fix lld-link warning about multiple entry points
    # if(WIN32)
    #   target_link_options(${tgt} PRIVATE -Xlinker /ENTRY:wWinMainCRTStartup)
    # endif()
  endif()

  # Installation rules
  install(TARGETS ${tgt}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
  )
endmacro()
