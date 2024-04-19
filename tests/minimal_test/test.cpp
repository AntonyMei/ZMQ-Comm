#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/pytypes.h>
#include <torch/torch.h>
#include <torch/extension.h>

namespace py = pybind11;

// TODO: remove this
torch::Tensor test_tensor() {
//    std::cout << "test_tensor" << std::endl;
//    torch::Tensor tensor = torch::ones({2, 2}); // simpler tensor
//    return tensor;
    auto options = torch::TensorOptions().dtype(torch::kFloat32).device(torch::kCPU);
    at::Tensor _de_kernel = torch::empty({2, 2}, options);
    return _de_kernel;
}

// Test function returning an integer
py::object test_integer() {
    return py::cast(42);
}

PYBIND11_MODULE(test, m) {
m.def("test_tensor", &test_tensor, "Return a simple tensor.");
m.def("test_integer", &test_integer, "Return an integer.");
}