add_library(cesium-language STATIC)
set_target_properties(cesium-language PROPERTIES
  OUTPUT_NAME "libcesium"
)

target_link_libraries(cesium-language
  PRIVATE
    ${CMAKE_DL_LIBS}  # For dlopen/dlsym on Unix systems
    yyjson
    utf8
)
