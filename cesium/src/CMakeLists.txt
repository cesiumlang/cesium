target_sources(cesium PRIVATE
  main.cpp
  json.cpp
  dynlib.cpp
  debug_utils.cpp
  cli_utils.cpp
  logging.cpp
  win32.cpp

  # Documentation core files
  doc/treesitter.cpp
  doc/markdowngen.cpp
  doc/docgen.cpp
  doc/doc_cli.cpp
  doc/cache.cpp

  # C++ language-specific files
  doc/cpp/docstrings.cpp
  doc/cpp/ts_ast_parser.cpp
  doc/cpp/ast_extractor.cpp
)
