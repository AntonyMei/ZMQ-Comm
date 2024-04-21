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
#include "inproc_queue.h"

// scheduler
std::string scheduler_type = "none";

// network
zmq::context_t context(1);
bool network_initialized = false;

// machine configs
bool machine_configs_initialized = false;
std::vector<Machine> machine_configs;

// cluster initialization
bool cluster_initialized = false;

// two signals to allow host_start_network_threads leave
bool scatter_in_main_loop = false;
bool gather_in_main_loop = false;

// Queue for launch requests
ThreadSafeQueue<MessageData> launch_queue;
ThreadSafeQueue<std::tuple<Header, int>> finish_queue;


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


void launch_request(
        const std::string &request_type,
        int request_id,
        int num_tokens,
        int max_num_tokens,
        std::vector<int> &token_ids,
        bool set_routing,
        const std::vector<int> &server_ids,
        const std::vector<int> &start_layer_ids,
        const std::vector<int> &end_layer_ids
) {
    Header header = Header();

    // set request type
    if (request_type == "prompt") {
        header.msg_type = MsgType::Prompt;
    } else if (request_type == "decode") {
        header.msg_type = MsgType::Decode;
        Assert(set_routing, "Must set routing in decode!");
    } else {
        Assert(false, "Unknown request type found!");
    }

    // set other fields
    header.creation_time = get_time();
    header.request_id = request_id;
    header.num_tokens = num_tokens;
    header.max_tokens = max_num_tokens;

    // set routing
    if (set_routing) {
        size_t num_stages = server_ids.size();
        for (size_t i = 0; i < num_stages; ++i) {
            header.add_stage(server_ids[i], start_layer_ids[i], end_layer_ids[i]);
        }
    }

    // build zmq message
    assert(token_ids.size() == 1 || (header.msg_type == MsgType::Prompt && token_ids.size() == num_tokens));
    zmq::message_t message(token_ids.data(), token_ids.size() * sizeof(int));

    // launch into queue for network transmission
    launch_queue.push(MessageData(header, std::move(message)));
}

// return val
// 1. request_id: List[int]
// 2. generated_id: List[int]
std::tuple<std::vector<int>, std::vector<int>> gather_finished_requests() {
    std::vector<int> request_ids;
    std::vector<int> generated_ids;

    // get all messages
    std::vector<std::tuple<Header, int>> new_messages = finish_queue.pop_all();

    for (auto &message: new_messages) {
        request_ids.push_back(std::get<0>(message).request_id);
        generated_ids.push_back(std::get<1>(message));
    }

    return {std::move(request_ids), std::move(generated_ids)};
}


void msg_scatter_thread(const std::string &host_ip) {
    log("Scatter", "Message scatter thread has successfully started!");

    // wait until machine configs are initialized
    while (!machine_configs_initialized) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    Machine host_machine;
    for (const auto &machine: machine_configs) {
        if (machine.machine_id == 0) {
            host_machine = machine;
            break;
        }
    }
    Assert(host_machine.ip_address == host_ip, "Host ip mismatch!");

    // get the output ips of host machine
    std::vector<std::pair<int, std::string>> output_id_ip;
    for (int machine_id: host_machine.out_nodes) {
        for (const auto &machine: machine_configs) {
            if (machine.machine_id == machine_id) {
                output_id_ip.emplace_back(machine_id, machine.ip_address);
            }
        }
    }
    for (const auto &id_ip: output_id_ip) {
        log("Scatter", "Output machine: id=[" + std::to_string(id_ip.first) + "], ip=[" + id_ip.second + "]");
    }

    // initialize the output sockets
    std::unordered_map<int, std::unique_ptr<PollServer>> output_sockets;
    for (const auto &id_ip: output_id_ip) {
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
        for (const auto &id_ip: output_id_ip) {
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
    for (const auto &id_ip: output_id_ip) {
        zmq::message_t complete_init_msg(init_msg_str.data(), init_msg_str.size());
        output_sockets[id_ip.first]->send(complete_init_header, complete_init_msg);
    }
    log("Scatter", "Successfully finished initialization, entering main loop!");
    scatter_in_main_loop = true;

    if (scheduler_type == "maxflow") {
        while (true) {
            // get all messages
            std::vector<MessageData> new_messages = launch_queue.pop_all();

            for (auto &message: new_messages) {
                if (message.header.msg_type == MsgType::Prompt || message.header.msg_type == MsgType::Decode) {
                    // for prompt and decode, just follow the route
                    int current_stage = message.header.current_stage;
                    int next_server_id = message.header.server_id[current_stage];

                    // send the request following the route
                    output_sockets[next_server_id]->send(message.header, message.buffer_msg);
                } else if (message.header.msg_type == MsgType::Terminate) {
                    return;
                } else {
                    Assert(false, "Bad message type: " + std::to_string((int) message.header.msg_type));
                }
            }
        }
    } else if (scheduler_type == "swarm") {
        // TODO: main loop of this thread
    } else if (scheduler_type == "random") {
        // TODO: main loop of this thread
    } else {
        Assert(false, "Bad scheduler type!");
    }
}


void msg_gather_thread(const std::string &host_ip) {
    log("Gather", "Message gather thread has successfully started!");

    // wait until machine configs are initialized
    while (!machine_configs_initialized) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    Machine host_machine;
    for (const auto &machine: machine_configs) {
        if (machine.machine_id == 0) {
            host_machine = machine;
            break;
        }
    }
    Assert(host_machine.ip_address == host_ip, "Host ip mismatch!");

    // get the output ips of host machine
    std::vector<std::pair<int, std::string>> input_id_ip;
    for (int machine_id: host_machine.in_nodes) {
        for (const auto &machine: machine_configs) {
            if (machine.machine_id == machine_id) {
                input_id_ip.emplace_back(machine_id, machine.ip_address);
            }
        }
    }
    for (const auto &id_ip: input_id_ip) {
        log("Gather", "Input machine: id=[" + std::to_string(id_ip.first) + "], ip=[" + id_ip.second + "]");
    }

    // initial the input sockets using Poller
    std::vector<std::string> input_addresses;
    for (const auto &id_ip: input_id_ip) {
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
    gather_in_main_loop = true;

    // main loop
    while (true) {
        // get the message
        zmq::message_t buffer_msg;
        Header header = poll_client.poll_once(buffer_msg, 10);
        if (header.msg_type == MsgType::Invalid) {
            continue;
        }

        if (header.msg_type == MsgType::Prompt || header.msg_type == MsgType::Decode) {
            // deserialize the message into a generated token id (int)
            Assert(buffer_msg.size() == sizeof(int), "Message should contain only one int!");
            int token_id = *(static_cast<int *>(buffer_msg.data()));

            // send the header and buffer to compute thread
            finish_queue.push({header, token_id});
        } else if (header.msg_type == MsgType::SwarmInfo) {
            // TODO: finish this branch
            Assert(scheduler_type == "swarm", "Received swarm info that should not exist!");
        } else if (header.msg_type == MsgType::Terminate) {
            break;
        }
    }
}


void host_start_network_threads(const std::string &config_broadcast_addr, const std::string &host_ip,
                                const std::string &config_file_path, const std::string &scheduler) {
    // config_broadcast_addr format: "tcp://10.128.0.53:5000"
    // host_ip format: "10.128.0.53"
    // scheduler is: (1) maxflow, (2) swarm, (3) random
    std::cout << "Config broadcast address: " << config_broadcast_addr << std::endl;
    std::cout << "Host IP address (local): " << host_ip << std::endl;
    std::cout << "Scheduler type: " << scheduler << std::endl;
    Assert(config_broadcast_addr.find(host_ip) != std::string::npos, "Host IP mismatch!");

    // check that network is not initialized
    Assert(!network_initialized, "Network threads have already been initialized!");
    network_initialized = true;

    // set scheduler
    Assert(scheduler == "swarm" || scheduler == "maxflow" || scheduler == "random", "Bad scheduler type");
    scheduler_type = scheduler;

    // creating threads
    std::thread t1(config_broadcast, config_broadcast_addr, config_file_path);
    std::thread t2(msg_scatter_thread, host_ip);
    std::thread t3(msg_gather_thread, host_ip);

    // detach from the main threads to avoid crash
    t1.detach();
    t2.detach();
    t3.detach();

    // wait until init finish to leave
    while (!scatter_in_main_loop || !gather_in_main_loop) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

#endif //ZMQ_COMM_HOST_H
