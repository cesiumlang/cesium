target_sources(cesium-backend PRIVATE
  treesitter.cpp
  markdowngen.cpp
  docgen.cpp
  doc_cli.cpp
  cache.cpp
  config.cpp
)
add_subdirectory(cpp)
