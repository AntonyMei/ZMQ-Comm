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
    m.def("start_network_threads", &host_start_network_threads, "Host: start network threads.");
    // Step 1: launch requests
    m.def("launch_request", &launch_request, "Host: launch request");
    // Step 2: gather finished requests
    m.def("gather_finished_requests", &gather_finished_requests, "Host: gather finished requests");
}
