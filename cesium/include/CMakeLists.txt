target_include_directories(cesium PUBLIC .)

configure_file(version.h.in version.h)
target_include_directories(cesium PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_sources(cesium PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/version.h)

target_sources(cesium PRIVATE
  json.h
  dynlib.h
  cli_utils.h
  debug_utils.h
  win32.h

  # Documentation core headers
  doc/treesitter.h
  doc/markdowngen.h
  doc/docgen.h
  doc/doc_cli.h

  # C++ language-specific headers
  doc/cpp/docstrings.h
  doc/cpp/ts_ast_parser.h
  doc/cpp/ast_extractor.h
)
