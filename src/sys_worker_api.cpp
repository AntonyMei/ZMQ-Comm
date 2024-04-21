//
// Created by meiyixuan on 2024/4/18.
//

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/pytypes.h>

#include "compute_worker.h"


PYBIND11_MODULE(llm_worker, m) {
    m.doc() = "LLM system worker";
    // Worker functions
    // Step 0: start network threads
    m.def("start_network_threads", &worker_start_network_threads, "Compute worker: start network threads.");
    m.def("get_model_start_end_idx", &get_model_start_end_idx, "Compute worker: get model start and end.");
    // Step 1: fetch new requests
    m.def("fetch_new_requests", &fetch_new_requests, "Compute worker: fetch new requests from receiver.");
    // Step 2: submit results
    m.def("submit_requests", &submit_requests, "Compute worker: submit results to sender.");
}