cmake_minimum_required(VERSION 3.26)
project(ZMQ_Comm)

set(CMAKE_CXX_STANDARD 14)

add_executable(myx_server tests/test_zmq_server.cpp)
add_executable(myx_client tests/test_zmq_client.cpp)
add_executable(myx_poll_server tests/test_poll_server.cpp)
add_executable(myx_poll_client tests/test_poll_client.cpp)

#find cppzmq wrapper, installed by make of cppzmq
find_package(cppzmq)
target_link_libraries(myx_server cppzmq)
target_link_libraries(myx_client cppzmq)
target_link_libraries(myx_poll_server cppzmq)
target_link_libraries(myx_poll_client cppzmq)
