# Replaces cmake/modules/get_clang_format.cmake (first on CMAKE_MODULE_PATH): upstream downloads
# .clang-format at configure time, which fails on the offline re-run; formatting is not needed here.
function(get_clang_format)
  message(STATUS "Skipping .clang-format download (stub)")
endfunction()
