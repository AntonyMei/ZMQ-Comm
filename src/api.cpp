//
// Created by meiyixuan on 2024/4/18.
//

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/pytypes.h>

#include "compute_worker.h"


void worker_start_network_threads() {
    // initialize zmq context
    zmq::context_t context(1);

    // Creating three threads
    std::thread t1(receiver_thread, std::ref(context));
    std::thread t2(sender_thread, std::ref(context));

    // detach from the main threads to avoid crash
    t1.detach();
    t2.detach();
}

PYBIND11_MODULE(llm_sys, m) {
    m.doc() = "LLM system";
    m.def("worker_start_network_threads", &worker_start_network_threads, "Compute worker: start network threads.");
}