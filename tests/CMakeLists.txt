
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    aux_source_directory(./ xkad_test_dir)
    # set(xkad_test_dir ./test_routing_table3.cc ./test_main.cc)
    add_executable(xkad_test ${xkad_test_dir})
    add_dependencies(xkad_test xkad)
    target_link_libraries(xkad_test xkad gtest gmock)

    if(XENABLE_CODE_COVERAGE)
        target_link_libraries(xkad_test gcov)
    endif()
endif()
