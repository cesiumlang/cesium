target_include_directories(cesium PUBLIC .)

configure_file(version.h.in version.h)
target_include_directories(cesium PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_sources(cesium PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/version.h)

# target_sources(cesium PRIVATE winmain.h)
