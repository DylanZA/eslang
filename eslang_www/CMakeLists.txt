file(GLOB all_files "*.cpp" "*.h")
add_library(eslang_www ${all_files})
target_link_libraries(eslang_www eslang ${ESLANG_BASE_LIBS})
