//
// Created by meiyixuan on 2024/4/18.
//

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/pytypes.h>

#include "compute_worker.h"


void worker_start_network_threads() {
    // check that network is not initialized
    Assert(!network_initialized, "Network threads have already been initialized!");
    network_initialized = true;

    // initialize zmq context
    zmq::context_t context(1);

    // Creating three threads
    std::thread t1(receiver_thread, std::ref(context));
    std::thread t2(sender_thread, std::ref(context));
    std::thread gc_thread1(message_gc);
    std::thread gc_thread2(tensor_gc);

    // detach from the main threads to avoid crash
    t1.detach();
    t2.detach();
    gc_thread1.detach();
    gc_thread2.detach();
}

PYBIND11_MODULE(llm_sys, m) {
    m.doc() = "LLM system";
    // Worker functions
    // Step 0: start network threads
    m.def("worker_start_network_threads", &worker_start_network_threads, "Compute worker: start network threads.");
    // Step 1: fetch new requests
    m.def("worker_fetch_new_requests", &fetch_new_requests, "Compute worker: fetch new requests from receiver.");
    // Step 2: submit results
    m.def("worker_submit_requests", &submit_requests, "Compute worker: submit results to sender.");
}