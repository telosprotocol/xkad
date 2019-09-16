
aux_source_directory(./ SRC)
add_executable(xkad_bench ${SRC})
add_dependencies(xkad_bench xkad)
target_link_libraries(xkad_bench xkad)
