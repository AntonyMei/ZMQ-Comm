//
// Created by meiyixuan on 2024/4/17.
//

#include <vector>
#include <torch/torch.h>  // Include the PyTorch header
#include <torch/extension.h>
#include <pybind11/pybind11.h>
#include <pybind11/embed.h>

using namespace torch;
namespace py = pybind11;


void call_python_function(const torch::Tensor& tensor) {
    py::scoped_interpreter guard{}; // start the interpreter and keep it alive

    py::module_ module = py::module_::import("test_torch");
    std::cout << "Successfully imported the module." << std::endl;
    auto proc_func = module.attr("print_tensor");

    // convert the tensor
    std::cout << "Converting tensor to python object." << std::endl;
    py::object py_tensor = py::cast(tensor);
    std::cout << "Converted tensor to python object." << std::endl;
    py::object result = proc_func(py_tensor);

    // get result
    auto result_tensor = result.cast<torch::Tensor>();
}


int main() {
    // create a buffer
    constexpr size_t buffer_size = 16 * 1024;
    std::vector<char> buffer(buffer_size, 'a');

    // Create a CPU tensor from the buffer
    auto options = torch::TensorOptions().dtype(torch::kFloat16);
    torch::Tensor tensor = torch::from_blob(buffer.data(), buffer.size(), options);
    std::cout << "Tensor successfully built on cpp side." << std::endl;

    // call the python function
    call_python_function(tensor);
    return 0;
}
