add_executable(cesium-exe)
ExeTgtDefaults(cesium-exe)
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT cesium-exe)
set_target_properties(cesium-exe PROPERTIES
  OUTPUT_NAME "cesium"
)

target_link_libraries(cesium-exe PRIVATE
  cesium-backend
  utf8
)

target_include_directories(cesium-exe PRIVATE
  ${CMAKE_BINARY_DIR}/include
)
target_sources(cesium-exe PRIVATE
  main.cpp
)
