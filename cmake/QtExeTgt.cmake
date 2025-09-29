include_guard(GLOBAL)

include(ExeTgtDefaults)

macro(QtExeTgt tgt)
  # set(CMAKE_AUTOMOC ON)
  # set(CMAKE_AUTOUIC ON)
  # set(CMAKE_AUTORCC ON)
  find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
  qt_standard_project_setup()
  qt_add_executable(${tgt})
  set_target_properties(${tgt} PROPERTIES
      WIN32_EXECUTABLE ON
      MACOSX_BUNDLE ON
  )

  ExeTgtDefaults(${tgt})
  # Clang-specific flags
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Fix lld-link warning about multiple entry points
    if(WIN32)
      target_link_options(${tgt} PRIVATE -Xlinker /ENTRY:wWinMainCRTStartup)
    endif()
  endif()

  target_link_libraries(${tgt} PRIVATE
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
    add_custom_command(TARGET ${tgt} POST_BUILD
      COMMAND $<IF:$<BOOL:${DEPLOY_TOOL}>,$<TARGET_FILE:${DEPLOY_TOOL}>,${DEPLOY_TOOL_FALLBACK}>
              --debug --no-translations $<TARGET_FILE:${tgt}>
      COMMENT "Deploying Qt application"
    )
  endif()
endmacro()
