cmake_minimum_required(VERSION 3.22)
project(ZMQ_Comm)

set(CMAKE_CXX_STANDARD 17)

# Find PythonLibs to get the include directories
# Note: "sudo apt install python3.10-dev" if not found
find_package(Python3 REQUIRED COMPONENTS Interpreter Development)
include_directories(${Python3_INCLUDE_DIRS})
link_libraries(${Python3_LIBRARIES})

# Find the PyTorch package
# Note: This is torch.utils.cmake_prefix_path in python.
list(APPEND CMAKE_PREFIX_PATH "/home/meiyixuan2000/miniconda3/envs/llm/lib/python3.10/site-packages/torch/share/cmake")
find_package(Torch REQUIRED)

# cpp zmq
find_package(cppzmq)

# server test
#add_executable(myx_server tests/test_zmq_server.cpp)
#add_executable(myx_client tests/test_zmq_client.cpp)
#target_link_libraries(myx_server cppzmq)
#target_link_libraries(myx_client cppzmq)

# poll test
#add_executable(myx_poll_server tests/test_poll_server.cpp)
#add_executable(myx_poll_client tests/test_poll_client.cpp)
#target_link_libraries(myx_poll_server cppzmq)
#target_link_libraries(myx_poll_client cppzmq)

# poller test
add_executable(packed_server tests/test_packed_server.cpp)
add_executable(packed_client tests/test_packed_client.cpp)
target_link_libraries(packed_server cppzmq)
target_link_libraries(packed_client cppzmq)

# test msg
add_executable(test_msg tests/test_msg.cpp)
target_link_libraries(test_msg cppzmq)

# test compute worker
add_executable(test_compute_worker tests/test_compute_worker.cpp)
target_link_libraries(test_compute_worker cppzmq)

# test cuda
add_executable(test_cuda tests/test_cuda.cpp)
target_link_libraries(test_cuda cppzmq)
target_link_libraries(test_cuda "${TORCH_LIBRARIES}")
target_include_directories(test_cuda PRIVATE ${TORCH_INCLUDE_DIRS})
