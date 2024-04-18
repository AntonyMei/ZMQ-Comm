//
// Created by meiyixuan on 2024/4/17.
//

#include <vector>
#include <torch/torch.h>  // Include the PyTorch header
#include <torch/extension.h>
#include <pybind11/pybind11.h>
#include <pybind11/embed.h>
#include <zmq.hpp>
#include <c10/util/Half.h>

#include "../src/utils.h"

using namespace torch;
namespace py = pybind11;


void call_python_function(const torch::Tensor& tensor) {
    py::scoped_interpreter guard{}; // start the interpreter and keep it alive

    py::module_ module = py::module_::import("test_torch");
    auto proc_func = module.attr("print_tensor");
    std::cout << "Python function successfully loaded." << std::endl;

    // convert the tensor
    // Calling takes 13 us (no copy, so independent of tensor size)
    auto start = get_time();
    py::object py_tensor = py::cast(tensor);
    py::object result = proc_func(py_tensor);
    auto result_tensor = result.cast<torch::Tensor>();
    auto end = get_time();
    std::cout << "Python function successfully called." << std::endl;
    std::cout << "Calling time: " << end - start << " us\n";
}


int main() {
    // create a buffer
    constexpr size_t buffer_size = 8192 * 2 * 1000;
    std::vector<char> buffer(buffer_size, 'a');
    zmq::message_t buffer_msg(buffer.data(), buffer.size());

    // Create a CPU tensor from the buffer
    // 80us for 8192 * 2 * 1000
    auto start = get_time();
    auto options = torch::TensorOptions().dtype(torch::kFloat16);
    auto* data = static_cast<c10::Half*>(buffer_msg.data());
    torch::Tensor tensor = torch::from_blob(data, (int)(buffer_msg.size() / 2), options);
    auto end = get_time();
    std::cout << "Tensor creation time: " << end - start << " us\n";
    std::cout << "Tensor size: " << tensor.numel() << std::endl;
    std::cout << "Tensor successfully built on cpp side." << std::endl;

    // call the python function
    call_python_function(tensor);
    return 0;
}
