target_include_directories(cesium PUBLIC .)

configure_file(version.h.in version.h)
target_include_directories(cesium PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_sources(cesium PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/version.h)

target_sources(cesium PRIVATE
  json.h
  treesitter.h
  dynlib.h
  javadoc.h
  ts_ast_parser.h
  markdowngen.h
  docgen.h
  doc_cli.h
)
