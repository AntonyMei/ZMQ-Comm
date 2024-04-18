//
// Created by meiyixuan on 2024/4/18.
//

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/pytypes.h>
#include <iostream>
#include <thread>

#include "../src//utils.h"

// receiver
void receiver_thread() {
    // initialize
    log("Receiver", "Receiver thread has successfully started!");

    int counter = 0;
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::cout << "Receiver: " << counter++ << "\n";
    }
}

// compute
void compute_thread() {
    // initialize
    log("Compute", "Compute thread has successfully started!");

    int counter = 0;
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::cout << "Compute: " << counter++ << "\n";
    }
}

void run_compute_worker() {
    // Creating three threads
    std::cout << "Creating two threads..." << std::endl;
    std::thread t1(receiver_thread);
    std::thread t2(compute_thread);
    std::cout << "Two threads created." << std::endl;

    t1.detach();
    t2.detach();
    std::cout << "Two threads detached." << std::endl;
}

PYBIND11_MODULE(example, m) {
    m.doc() = "Pybind11 example plugin"; // Optional docstring
    m.def("run", &run_compute_worker, "Run two threads in background.");
}
