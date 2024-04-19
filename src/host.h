//
// Created by meiyixuan on 2024/4/19.
//

#ifndef ZMQ_COMM_HOST_H
#define ZMQ_COMM_HOST_H

#include <zmq.hpp>
#include <iostream>
#include <thread>

#include "utils.h"
#include "config_parser.h"

// network
zmq::context_t context(1);
bool network_initialized = false;


void config_broadcast(const std::string &config_broadcast_addr, const std::string &config_file_path) {
    // initialize
    log("Config Broadcast", "Config broadcast thread has successfully started!");

    // initialize config broadcast socket
    zmq::socket_t init_socket(context, zmq::socket_type::rep);
    init_socket.bind(config_broadcast_addr);
    log("Config Broadcast", "Successfully bind to address " + config_broadcast_addr);

    // read the config file and serialize it
    std::vector<Machine> machine_configs = read_config(config_file_path);
    std::string serialized_config = serialize_vector_of_machines(machine_configs);

    // main loop
    while (true) {
        // receive one config request
        zmq::message_t request;
        auto rc = init_socket.recv(request, zmq::recv_flags::none);
        Assert(rc.has_value(), "Failed to receive the initialization message!");

        // print the initialization message
        std::string request_str(static_cast<char *>(request.data()), request.size());
        log("Config Broadcast", "Received request from ip [" + request_str + "]");

        // reply with some dummy message
        zmq::message_t reply_msg(serialized_config.data(), serialized_config.size());
        init_socket.send(reply_msg, zmq::send_flags::none);
    }
}


void host_start_network_threads(const std::string &config_broadcast_addr, const std::string &host_ip,
                                const std::string &config_file_path) {
    // config_broadcast_addr format: "tcp://10.128.0.53:5000"
    // host_ip format: "10.128.0.53"
    std::cout << "Config broadcast address: " << config_broadcast_addr << std::endl;
    std::cout << "Host IP address (local): " << host_ip << std::endl;
    Assert(config_broadcast_addr.find(host_ip) != std::string::npos, "Host IP mismatch!");

    // check that network is not initialized
    Assert(!network_initialized, "Network threads have already been initialized!");
    network_initialized = true;

    // creating threads
    std::thread t1(config_broadcast, config_broadcast_addr, config_file_path);

    // detach from the main threads to avoid crash
    t1.detach();
}

#endif //ZMQ_COMM_HOST_H
