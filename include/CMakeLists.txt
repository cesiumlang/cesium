include_directories(${CMAKE_CURRENT_SOURCE_DIR})

configure_file(version.h.in version.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
# target_sources(cesium-backend PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/version.h)
