file(GLOB all_files "*.cpp" "*.h")
add_library(eslang_io ${all_files})
target_link_libraries(eslang_io eslang ${ESLANG_BASE_LIBS})
