cmake_minimum_required(VERSION 3.8)

include_directories(SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/proto)

aux_source_directory(./src xkad_src)
aux_source_directory(./proto xkad_src)
add_library(xkad ${xkad_src})

add_dependencies(xkad xpbase xtransport xledger)
target_link_libraries(xkad xpbase xtransport xledger protobuf)

if(XENABLE_TESTS)
    add_subdirectory(tests)
    add_subdirectory(bench)
endif()
