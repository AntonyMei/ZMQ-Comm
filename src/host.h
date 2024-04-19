//
// Created by meiyixuan on 2024/4/19.
//

#ifndef ZMQ_COMM_HOST_H
#define ZMQ_COMM_HOST_H

#include <zmq.hpp>
#include <iostream>
#include <thread>

#include "utils.h"

// network
zmq::context_t context(1);
bool network_initialized = false;


void config_broadcast(const std::string &host_addr, const std::string &self_ip) {
    // initialize
    log("Config Broadcast", "Config broadcast thread has successfully started!");

    // get configuration from host
    zmq::socket_t init_socket(context, zmq::socket_type::rep);
    init_socket.bind(host_addr);
    log("Config Broadcast", "Successfully bind to address " + host_addr);

    // main loop
    while (true) {
        // receive one config request
        zmq::message_t request;
        auto rc = init_socket.recv(request, zmq::recv_flags::none);
        Assert(rc.has_value(), "Failed to receive the initialization message!");

        // TODO: dummy
        // print the initialization message
        std::string request_str(static_cast<char *>(request.data()), request.size());
        std::cout << "Received config request: " << request_str << std::endl;
        // reply with some dummy message
        std::string reply_str = "Config reply";
        zmq::message_t reply_msg(reply_str.data(), reply_str.size());
        init_socket.send(reply_msg, zmq::send_flags::none);
    }
}



void host_start_network_threads(const std::string &host_addr, const std::string &self_ip) {
    // host_addr format: "tcp://10.128.0.53:5000"
    // self_ip format: "10.128.0.53"
    std::cout << "Host connection address: " << host_addr << std::endl;
    std::cout << "Self IP address (local): " << self_ip << std::endl;

    // check that network is not initialized
    Assert(!network_initialized, "Network threads have already been initialized!");
    network_initialized = true;

    // creating threads
    std::thread t1(config_broadcast, host_addr, self_ip);

    // detach from the main threads to avoid crash
    t1.detach();
}

#endif //ZMQ_COMM_HOST_H
