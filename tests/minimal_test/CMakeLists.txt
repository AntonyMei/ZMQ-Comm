cmake_minimum_required(VERSION 3.22)
project(minimal_test)

set(CMAKE_CXX_STANDARD 17)

include(utils.cmake)

# Find PythonLibs to get the include directories
# Note: "sudo apt install python3.10-dev" if not found
find_package(Python3 REQUIRED COMPONENTS Interpreter Development)
include_directories(${Python3_INCLUDE_DIRS})
link_libraries(${Python3_LIBRARIES})

# Find the pybind11 package
# Note: "sudo apt-get install pybind11-dev" if not found
find_package(pybind11 REQUIRED)

# Find the PyTorch package
# Note: This is torch.utils.cmake_prefix_path in python.
#list(APPEND CMAKE_PREFIX_PATH "/home/meiyixuan2000/miniconda3/envs/llm/lib/python3.10/site-packages/torch/share/cmake")
list(APPEND CMAKE_PREFIX_PATH "/usr/local/lib/python3.10/dist-packages/torch")
#append_cmake_prefix_path("torch" "torch.utils.cmake_prefix_path")
find_package(Torch REQUIRED)

# Find libtorch_python.so, this is not included in the torch package by default
find_library(torch_python_LIBRARY torch_python PATHS "${TORCH_INSTALL_PREFIX}/lib")

# test c->py api
pybind11_add_module(test MODULE test.cpp)
#target_include_directories(test PRIVATE ${TORCH_INCLUDE_DIRS} ${PYTHON_INCLUDE_DIRS})
#target_link_libraries(test PRIVATE ${PYTHON_LIBRARIES})
target_include_directories(test PRIVATE ${TORCH_INCLUDE_DIRS})
target_link_libraries(test PRIVATE ${TORCH_LIBRARIES})
target_link_libraries(test PRIVATE ${torch_python_LIBRARY})
#target_link_libraries(test PRIVATE pybind11::pybind11)
