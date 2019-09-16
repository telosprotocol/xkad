remove_definitions(
    -DGOSSIP_WITH_WROUTER
)

aux_source_directory(./ xkad_demo_dir)
add_executable(xkad_demo ${xkad_demo_dir})

add_dependencies(xkad_demo xgossip xkad xxbase)

target_link_libraries(xkad_demo
    xgossip
    xkad 
    xtransport
    xpbase
    xcrypto
    xutility
    xxbase
    xledger
    protobuf
    -lgtest
    -lgmock
    -lpthread -ldl
    -lrt
)

