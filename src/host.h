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
#include "poller.h"

// network
zmq::context_t context(1);
bool network_initialized = false;

// machine configs
bool machine_configs_initialized = false;
std::vector<Machine> machine_configs;


void config_broadcast(const std::string &config_broadcast_addr, const std::string &config_file_path) {
    // initialize
    log("Config Broadcast", "Config broadcast thread has successfully started!");

    // initialize config broadcast socket
    zmq::socket_t init_socket(context, zmq::socket_type::rep);
    init_socket.bind(config_broadcast_addr);
    log("Config Broadcast", "Successfully bind to address " + config_broadcast_addr);

    // read the config file and serialize it
    machine_configs = read_config(config_file_path);
    std::string serialized_config = serialize_vector_of_machines(machine_configs);
    machine_configs_initialized = true;

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


void msg_scatter_thread(const std::string &host_ip) {
    log("MSG Scatter", "Message scatter thread has successfully started!");

    // wait until machine configs are initialized
    while (!machine_configs_initialized) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    Machine host_machine;
    for (const auto& machine: machine_configs) {
        if (machine.machine_id == 0) {
            host_machine = machine;
            break;
        }
    }
    Assert(host_machine.ip_address == host_ip, "Host ip mismatch!");

    // get the output ips of host machine
    std::vector<std::pair<int, std::string>> output_id_ip;
    for (int machine_id : host_machine.out_nodes) {
        for (const auto& machine: machine_configs) {
            if (machine.machine_id == machine_id) {
                output_id_ip.emplace_back(machine_id, machine.ip_address);
            }
        }
    }
    for (const auto& id_ip: output_id_ip) {
        log("Msg Scatter", "Output machine: id=[" + std::to_string(id_ip.first) + "], ip=[" + id_ip.second + "]");
    }

    // initialize the output sockets
    std::unordered_map<int, std::unique_ptr<PollServer>> output_sockets;
    for (const auto& id_ip: output_id_ip) {
        std::string bind_address = "tcp://" + host_ip + ":" + std::to_string(BASE_PORT + id_ip.first);
        output_sockets[id_ip.first] = std::make_unique<PollServer>(context, bind_address);
    }

    // TODO: main loop of this thread
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}


void msg_gather_thread(const std::string &host_ip) {
    log("MSG Gather", "Message gather thread has successfully started!");

    // wait until machine configs are initialized
    while (!machine_configs_initialized) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    Machine host_machine;
    for (const auto& machine: machine_configs) {
        if (machine.machine_id == 0) {
            host_machine = machine;
            break;
        }
    }
    Assert(host_machine.ip_address == host_ip, "Host ip mismatch!");

    // get the output ips of host machine
    std::vector<std::pair<int, std::string>> input_id_ip;
    for (int machine_id : host_machine.in_nodes) {
        for (const auto& machine: machine_configs) {
            if (machine.machine_id == machine_id) {
                input_id_ip.emplace_back(machine_id, machine.ip_address);
            }
        }
    }
    for (const auto& id_ip: input_id_ip) {
        log("Msg Gather", "Input machine: id=[" + std::to_string(id_ip.first) + "], ip=[" + id_ip.second + "]");
    }

    // initial the input sockets using Poller
    std::vector<std::string> input_addresses;
    for (const auto& id_ip: input_id_ip) {
        std::string address = "tcp://" + id_ip.second + ":" + std::to_string(BASE_PORT + host_machine.machine_id);
        input_addresses.emplace_back(address);
    }
    PollingClient poll_client = PollingClient(context, input_addresses);

    // TODO: main loop of the thread
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
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
    std::thread t2(msg_scatter_thread, host_ip);
    std::thread t3(msg_gather_thread, host_ip);

    // detach from the main threads to avoid crash
    t1.detach();
    t2.detach();
    t3.detach();
}

#endif //ZMQ_COMM_HOST_H
