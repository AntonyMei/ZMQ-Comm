//
// Created by meiyixuan on 2024/4/18.
//

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/pytypes.h>

#include "host.h"


PYBIND11_MODULE(llm_host, m) {
    m.doc() = "LLM system host";
    // Host functions
    // Step 0: start network threads
    m.def("host_start_network_threads", &host_start_network_threads, "Host: start network threads.");
}
