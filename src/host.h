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

// cluster initialization
bool cluster_initialized = false;


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
    log("Scatter", "Message scatter thread has successfully started!");

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
        log("Scatter", "Output machine: id=[" + std::to_string(id_ip.first) + "], ip=[" + id_ip.second + "]");
    }

    // initialize the output sockets
    std::unordered_map<int, std::unique_ptr<PollServer>> output_sockets;
    for (const auto& id_ip: output_id_ip) {
        std::string bind_address = "tcp://" + host_ip + ":" + std::to_string(BASE_PORT + id_ip.first);
        output_sockets[id_ip.first] = std::make_unique<PollServer>(context, bind_address);
    }

    // send out initialization messages to make sure every machine is ready
    // prepare header and buffer ms (this can be reused)
    Header init_header = Header();
    init_header.msg_type = MsgType::Init;
    init_header.creation_time = get_time();
    std::string init_msg_str = std::to_string(host_machine.machine_id);  // send the machine id out
    // send out the init messages
    log("Scatter", "Sending out initialization messages to the cluster!");
    while (!cluster_initialized) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        for (const auto& id_ip: output_id_ip) {
            zmq::message_t init_msg(init_msg_str.data(), init_msg_str.size());
            output_sockets[id_ip.first]->send(init_header, init_msg);
        }
    }
    log("Scatter", "Cluster is initialized, sending out complete initialization messages!");
    // send out the complete init messages
    // send out once as all machines are sure to receive the message
    Header complete_init_header = Header();
    complete_init_header.msg_type = MsgType::InitComplete;
    complete_init_header.creation_time = get_time();
    for (const auto& id_ip: output_id_ip) {
        zmq::message_t complete_init_msg(init_msg_str.data(), init_msg_str.size());
        output_sockets[id_ip.first]->send(complete_init_header, complete_init_msg);
    }
    log("Scatter", "Successfully finished initialization, entering main loop!");


    // TODO: main loop of this thread
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}


void msg_gather_thread(const std::string &host_ip) {
    log("Gather", "Message gather thread has successfully started!");

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
        log("Gather", "Input machine: id=[" + std::to_string(id_ip.first) + "], ip=[" + id_ip.second + "]");
    }

    // initial the input sockets using Poller
    std::vector<std::string> input_addresses;
    for (const auto& id_ip: input_id_ip) {
        std::string address = "tcp://" + id_ip.second + ":" + std::to_string(BASE_PORT + host_machine.machine_id);
        input_addresses.emplace_back(address);
    }
    PollingClient poll_client = PollingClient(context, input_addresses);

    // wait until all input ports are properly initialized
    // a set of machines that are not ready
    std::unordered_set<int> input_machines_not_ready;
    for (const auto &id_ip: input_id_ip) {
        input_machines_not_ready.insert(id_ip.first);
    }
    // loop until all input machines are ready
    while (!input_machines_not_ready.empty()) {
        // receive a message
        zmq::message_t input_msg;
        Header header = poll_client.poll_once(input_msg, 100);
        if (header.msg_type == MsgType::Invalid) {
            continue;
        }
        Assert(header.msg_type == MsgType::Init, "Received non-init message!");

        // convert the message to string
        std::string input_msg_str(static_cast<char *>(input_msg.data()), input_msg.size());
        int input_machine_id = std::stoi(input_msg_str);

        // remove if still in the set
        if (input_machines_not_ready.find(input_machine_id) != input_machines_not_ready.end()) {
            input_machines_not_ready.erase(input_machine_id);
            log("Gather", "Input machine id=[" + std::to_string(input_machine_id) + "] is ready!");
            log("Gather", "Remaining input machines: " + std::to_string(input_machines_not_ready.size()) + "!");
        }
    }
    // mark as ready
    cluster_initialized = true;
    log("Gather", "All input ports are properly initialized!");

    // receive complete init signal from all input machines
    // a set of machines that are not ready
    std::unordered_set<int> input_machines_not_complete;
    for (const auto &id_ip: input_id_ip) {
        input_machines_not_complete.insert(id_ip.first);
    }
    // loop until all input machines are complete
    while (!input_machines_not_complete.empty()) {
        // receive a message
        zmq::message_t input_msg;
        Header header = poll_client.poll_once(input_msg, 100);
        if (header.msg_type == MsgType::Invalid) {
            continue;
        }
        Assert(header.msg_type == MsgType::InitComplete || header.msg_type == MsgType::Init,
               "Received non-init-related message!");

        // convert the message to string
        std::string input_msg_str(static_cast<char *>(input_msg.data()), input_msg.size());
        int input_machine_id = std::stoi(input_msg_str);

        // remove the machine from the set based on message type
        Assert(input_machines_not_complete.find(input_machine_id) != input_machines_not_complete.end(),
               "Machine already marked as complete!");
        if (header.msg_type == MsgType::InitComplete) {
            input_machines_not_complete.erase(input_machine_id);
            log("Gather", "Input machine id=[" + std::to_string(input_machine_id) + "] is complete!");
            log("Gather", "Remaining input machines: " + std::to_string(input_machines_not_complete.size()) + "!");
        }
    }
    log("Gather", "Successfully finished initialization, entering main loop!");

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
