
SET(SYNC_CLI_TEST_SRC
    sync_client_test.cpp
    timespec_operations.h
    timespec_operations.c
)

SET(SRV_TEST_SRC
    server_test.cpp
)

#SET(CXXSTANDARD "-std=c++11")

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CXXSTANDARD}")

#add_subdirectory(posix_serial_rtu)
#add_subdirectory(rtu)

include_directories(../implementations/include)

add_executable(emb_sync_client_test ${SYNC_CLI_TEST_SRC})

add_executable(emb_srv_test ${SRV_TEST_SRC})

target_link_libraries(emb_srv_test
    emodbus-base
    emodbus-server
    emodbus-cpp
    pthread
    emodbus-impl-posix
)


target_link_libraries(emb_sync_client_test
    emodbus-base
    emodbus-client
    emodbus-cpp
    pthread
    emodbus-impl-posix
    emodbus-cpp-impl-posix
    event_pthreads
)

#add_executable(ev_srv event_server.c)

#target_link_libraries(ev_srv event_core)

add_executable(emb_tty2tty tty2tty.c)

