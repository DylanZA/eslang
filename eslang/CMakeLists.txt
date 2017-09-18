file(GLOB all_files "*.cpp"  "*.h")
add_library(eslang ${all_files})
target_link_libraries(eslang ${ESLANG_BASE_LIBS})