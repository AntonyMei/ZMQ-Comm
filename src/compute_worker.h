//
// Created by meiyixuan on 2024/4/17.
//

#ifndef ZMQ_COMM_COMPUTE_WORKER_H
#define ZMQ_COMM_COMPUTE_WORKER_H

#include <iostream>
#include <thread>
#include <zmq.hpp>
#include <torch/torch.h>
#include <torch/extension.h>
#include <random>

#include "config_parser.h"
#include "utils.h"
#include "const.h"
#include "msg.h"
#include "inproc_queue.h"
#include "poller.h"
#include "swarm.h"

// scheduler
std::string scheduler_type = "none";

// machine config
bool machine_configs_initialized = false;
std::vector<Machine> machine_configs;

// global queue
zmq::context_t context(1);
bool network_initialized = false;
ThreadSafeQueue<MessageData> recv_compute_queue;
ThreadSafeQueue<MessageData> compute_send_queue;

// the requests that are in compute
std::unordered_map<int, std::shared_ptr<MessageData>> requests_on_the_fly;

// gc queues
ThreadSafeQueue<std::shared_ptr<MessageData>> requests_to_release;
ThreadSafeQueue<std::shared_ptr<torch::Tensor>> tensors_to_release;

// signals
bool input_port_initialized = false;
bool stop_sending_init = false;

// two signals to allow worker_start_network_threads leave
bool receiver_in_main_loop = false;
bool sender_in_main_loop = false;

// model start and end layer idx: [start, end)
int current_machine_id = -1;
int model_start_layer_index = -1;
int model_end_layer_index = -1;
bool is_last_layer = false;


// receiver
void receiver_thread(const std::string &config_broadcast_addr, const std::string &worker_ip) {
    // initialize
    log("Receiver", "Receiver thread has successfully started!");

    // get configuration from host
    zmq::socket_t init_socket(context, zmq::socket_type::req);
    init_socket.connect(config_broadcast_addr);
    zmq::message_t init_msg(worker_ip.data(), worker_ip.size());
    zmq::message_t init_reply_msg;
    init_socket.send(init_msg, zmq::send_flags::none);
    auto rc = init_socket.recv(init_reply_msg, zmq::recv_flags::none);
    Assert(rc.has_value(), "Failed to receive the initialization message!");

    // deserialize the initialization message
    std::string init_reply_str(static_cast<char *>(init_reply_msg.data()), init_reply_msg.size());
    machine_configs = deserialize_vector_of_machines(init_reply_str);
    machine_configs_initialized = true;
    log("Receiver", "Received machine configs from host!");
    for (const auto &machine: machine_configs) {
        print_machine(machine);
    }
    log("Receiver", "Above is the whole table of received configs!");

    // get the machine config for the current worker
    Machine current_machine;
    for (const auto &machine: machine_configs) {
        if (machine.ip_address == worker_ip) {
            current_machine = machine;
            break;
        }
    }
    Assert(current_machine.machine_id != -1, "Could not find config for worker!");
    current_machine_id = current_machine.machine_id;
    model_start_layer_index = current_machine.start_layer;
    model_end_layer_index = current_machine.end_layer;

    // determine whether the node's output is the last layer
    // if the node has only one output and the output is 0 (host), then it is the last layer
    if (current_machine.out_nodes.size() == 1 && current_machine.out_nodes[0] == 0) {
        is_last_layer = true;
    }

    // get input machine id and ip pairs
    std::vector<std::pair<int, std::string>> input_id_ip;
    for (int machine_id: current_machine.in_nodes) {
        for (const auto &machine: machine_configs) {
            if (machine.machine_id == machine_id) {
                input_id_ip.emplace_back(machine_id, machine.ip_address);
            }
        }
    }
    for (const auto &id_ip: input_id_ip) {
        log("Receiver", "Input machine: id=[" + std::to_string(id_ip.first) + "], ip=[" + id_ip.second + "]");
    }

    // initialize the polling client
    std::vector<std::string> input_addresses;
    for (const auto &id_ip: input_id_ip) {
        std::string address = "tcp://" + id_ip.second + ":" + std::to_string(BASE_PORT + current_machine.machine_id);
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
            log("Receiver", "Input machine id=[" + std::to_string(input_machine_id) + "] is ready!");
            log("Receiver", "Remaining input machines: " + std::to_string(input_machines_not_ready.size()) + "!");
        }
    }
    // mark as ready
    input_port_initialized = true;
    log("Receiver", "All input ports are properly initialized!");

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
            log("Receiver", "Input machine id=[" + std::to_string(input_machine_id) + "] is complete!");
            log("Receiver", "Remaining input machines: " + std::to_string(input_machines_not_complete.size()) + "!");
        }
    }
    // mark as ready
    stop_sending_init = true;
    log("Receiver", "Successfully finished initialization, entering main loop!");
    receiver_in_main_loop = true;


    // main loop
    if (scheduler_type == "maxflow") {
        while (true) {
            // get the message
            zmq::message_t buffer_msg;
            Header header = poll_client.poll_once(buffer_msg, 10);
            if (header.msg_type == MsgType::Invalid) {
                continue;
            }

            if (header.msg_type == MsgType::Prompt || header.msg_type == MsgType::Decode) {
                // send the header and buffer to compute thread
                Assert(header.server_id[header.current_stage] == current_machine_id, "Mis-routed request!");
                recv_compute_queue.push(MessageData(header, std::move(buffer_msg)));
            } else if (header.msg_type == MsgType::Terminate) {
                break;
            } else {
                Assert(false, "Unknown message type!");
            }
        }
    } else if (scheduler_type == "swarm") {
        while (true) {
            // get the message
            zmq::message_t buffer_msg;
            Header header = poll_client.poll_once(buffer_msg, 10);
            if (header.msg_type == MsgType::Invalid) {
                continue;
            }

            if (header.msg_type == MsgType::Prompt) {
                // need to determine the layers to infer before sending into the queue
                if (header.current_stage == 0) {
                    header.start_layer_idx[header.current_stage] = 0;
                } else {
                    header.start_layer_idx[header.current_stage] = header.end_layer_idx[header.current_stage - 1];
                }
                header.end_layer_idx[header.current_stage] = model_end_layer_index;
                Assert(model_start_layer_index <= header.start_layer_idx[header.current_stage], "Bad infer setting!");
                Assert(header.start_layer_idx[header.current_stage] < model_end_layer_index, "Bad infer setting!");

                // send the header and buffer to compute thread
                Assert(header.server_id[header.current_stage] == current_machine_id, "Mis-routed request!");
                recv_compute_queue.push(MessageData(header, std::move(buffer_msg)));
            } else if (header.msg_type == MsgType::Decode) {
                // send the header and buffer to compute thread
                Assert(header.server_id[header.current_stage] == current_machine_id, "Mis-routed request!");
                recv_compute_queue.push(MessageData(header, std::move(buffer_msg)));
            } else if (header.msg_type == MsgType::SwarmInfo) {
                Assert(header.server_id[header.current_stage] == current_machine_id, "Mis-routed request!");

                // increase the stage counter and send directly to the sender
                header.current_stage++;
                compute_send_queue.push(MessageData(header, std::move(buffer_msg)));
            } else if (header.msg_type == MsgType::Terminate) {
                break;
            } else {
                Assert(false, "Unknown message type!");
            }
        }
    } else if (scheduler_type == "random") {
        while (true) {
            // get the message
            zmq::message_t buffer_msg;
            Header header = poll_client.poll_once(buffer_msg, 10);
            if (header.msg_type == MsgType::Invalid) {
                continue;
            }

            if (header.msg_type == MsgType::Prompt) {
                // need to determine the layers to infer before sending into the queue
                if (header.current_stage == 0) {
                    header.start_layer_idx[header.current_stage] = 0;
                } else {
                    header.start_layer_idx[header.current_stage] = header.end_layer_idx[header.current_stage - 1];
                }
                header.end_layer_idx[header.current_stage] = model_end_layer_index;
                Assert(model_start_layer_index <= header.start_layer_idx[header.current_stage], "Bad infer setting!");
                Assert(header.start_layer_idx[header.current_stage] < model_end_layer_index, "Bad infer setting!");

                // send the header and buffer to compute thread
                Assert(header.server_id[header.current_stage] == current_machine_id, "Mis-routed request!");
                recv_compute_queue.push(MessageData(header, std::move(buffer_msg)));
            } else if (header.msg_type == MsgType::Decode) {
                // send the header and buffer to compute thread
                Assert(header.server_id[header.current_stage] == current_machine_id, "Mis-routed request!");
                recv_compute_queue.push(MessageData(header, std::move(buffer_msg)));
            } else if (header.msg_type == MsgType::Terminate) {
                break;
            } else {
                Assert(false, "Unknown message type!");
            }
        }
    } else {
        Assert(false, "Unknown scheduler type!");
    }
}

// compute - step 0: get which layers are on this node
std::tuple<int, int, bool> get_model_start_end_idx() {
    return {model_start_layer_index, model_end_layer_index, is_last_layer};
}


// compute - step 1: fetch
// Return values:
//  1. request_ids: List[int]
//  2. is_prompt: List[bool]
//  3. start_layer_idx: List[int]
//  4. end_layer_idx: List[int]
//  5. num_tokens: List[int]
//  6. max_tokens: List[int]
//  7. offsets: List[int]
//  8. lengths: List[int]
//  9. is_token_tensor: List[bool], if true, find things in token tensor, o.w. find things in activation tensor
//  10. token_tensor: Torch.Tensor (torch.int32), will be None if empty
//  11. activation_tensor: Torch.Tensor (torch.FP16), will be None if empty
std::tuple<std::vector<int>, std::vector<bool>, std::vector<int>, std::vector<int>, std::vector<int>,
        std::vector<int>, std::vector<int>, std::vector<int>, std::vector<bool>, torch::Tensor,
        torch::Tensor> fetch_new_requests() {
    // read all messages from the receiver
    std::vector<MessageData> messages = recv_compute_queue.pop_all();

    // process the messages into torch tensors
    std::vector<torch::Tensor> token_tensors;       // int32
    std::vector<torch::Tensor> activation_tensors;  // float16
    // location
    std::vector<bool> is_token_tensor;
    std::vector<int> offsets;
    std::vector<int> lengths;
    int token_offset = 0;
    int activation_offset = 0;
    // parse
    for (auto &message: messages) {
        // get the first layer
        int current_stage = message.header.current_stage;
        int start_layer_idx = message.header.start_layer_idx[current_stage];

        if (start_layer_idx == 0) {
            // buffer contains tokens, should be int32
            auto options = torch::TensorOptions().dtype(torch::kInt32);
            auto *data = static_cast<int *>(message.buffer_msg.data());  // use int as the type
            auto length = static_cast<int>(message.buffer_msg.size() / sizeof(int));
            torch::Tensor tensor = torch::from_blob(data, {length}, options);

            // give location
            is_token_tensor.emplace_back(true);
            offsets.emplace_back(token_offset);
            lengths.emplace_back(tensor.numel());
            token_offset += (int) tensor.numel();

            // save the tensor
            token_tensors.emplace_back(std::move(tensor));
        } else {
            // buffer contains activations, should be fp16
            auto options = torch::TensorOptions().dtype(torch::kFloat16);
            auto *data = static_cast<c10::Half *>(message.buffer_msg.data());
            auto length = static_cast<int>(message.buffer_msg.size() / 2);
            torch::Tensor tensor = torch::from_blob(data, {length}, options);

            // give location
            is_token_tensor.emplace_back(false);
            offsets.emplace_back(activation_offset);
            lengths.emplace_back(tensor.numel());
            activation_offset += (int) tensor.numel();

            // save the tensor
            activation_tensors.emplace_back(std::move(tensor));
        }
    }

    // process the header
    std::vector<int> request_ids;
    std::vector<bool> is_prompt;
    std::vector<int> start_layer_idx;
    std::vector<int> end_layer_idx;
    std::vector<int> num_tokens;
    std::vector<int> max_tokens;
    for (const auto &message: messages) {
        request_ids.emplace_back(message.header.request_id);
        is_prompt.emplace_back(message.header.msg_type == MsgType::Prompt);
        start_layer_idx.emplace_back(message.header.start_layer_idx[message.header.current_stage]);
        end_layer_idx.emplace_back(message.header.end_layer_idx[message.header.current_stage]);
        num_tokens.emplace_back(message.header.num_tokens);
        max_tokens.emplace_back(message.header.max_tokens);
    }

    // save the flying batch
    for (auto &message: messages) {
        auto request_id = message.header.request_id;
        Assert(requests_on_the_fly.find(request_id) == requests_on_the_fly.end(), "Request already exists!");
        requests_on_the_fly[request_id] = std::make_shared<MessageData>(std::move(message));
    }

    // move the result tensor to GPU
    // if no tensors, return an empty tensor (on cpu)
    torch::Tensor token_tensor_gpu;
    if (!token_tensors.empty()) {
        torch::Tensor cat_token_tensors = torch::cat(token_tensors, 0);
        torch::Device device(torch::kCUDA, 0);
        token_tensor_gpu = cat_token_tensors.to(device);
    }
    torch::Tensor activation_tensor_gpu;
    if (!activation_tensors.empty()) {
        torch::Tensor cat_activation_tensors = torch::cat(activation_tensors, 0);
        torch::Device device(torch::kCUDA, 0);
        activation_tensor_gpu = cat_activation_tensors.to(device);
    }

    // return the tensors
    return {std::move(request_ids), std::move(is_prompt), std::move(start_layer_idx), std::move(end_layer_idx),
            std::move(num_tokens), std::move(max_tokens), std::move(offsets), std::move(lengths),
            std::move(is_token_tensor), std::move(token_tensor_gpu), std::move(activation_tensor_gpu)};
}


// Compute - step 2: submit
// Args:
//  1. request_ids: List[int]
//  2. offsets: List[int] (in number of elements)
//  3. lengths: List[int] (in number of elements)
//  4. result_tensor: Torch.Tensor (a cpu tensor)
//      if not last layer: type: torch.float16: activations
//      if last layer: type: torch.int32: generated token ids
void submit_requests(std::vector<int> &request_ids,
                     std::vector<int> &offsets,
                     std::vector<int> &lengths,
                     torch::Tensor &result_tensor) {
    Assert(request_ids.size() == offsets.size() && offsets.size() == lengths.size(),
           "Request ids, offsets, and lengths should have the same size!");
    size_t submit_num = request_ids.size();

    // move the tensor to the hold queue
    tensors_to_release.push(std::make_shared<torch::Tensor>(result_tensor));

    // fetch the requests from the flying batch
    for (size_t i = 0; i < submit_num; i++) {
        // get basic info
        auto request_id = request_ids[i];
        auto offset = offsets[i];
        auto length = lengths[i];

        // get the message and remove it from the flying batch
        auto message = requests_on_the_fly[request_id];

        // update the header
        message->header.current_stage += 1;

        // build new zmq message and push into queue
        if (!is_last_layer) {
            // send the activations
            auto *start_ptr = result_tensor.data_ptr<c10::Half>() + offset;
            zmq::message_t new_buf_msg(start_ptr, length * result_tensor.element_size(), custom_free, nullptr);
            auto new_message = MessageData(message->header, std::move(new_buf_msg));
            compute_send_queue.push(std::move(new_message));
        } else {
            // send the token ids
            auto *start_ptr = result_tensor.data_ptr<int>() + offset;
            zmq::message_t new_buf_msg(start_ptr, length * result_tensor.element_size(), custom_free, nullptr);
            auto new_message = MessageData(message->header, std::move(new_buf_msg));
            compute_send_queue.push(std::move(new_message));
        }

        // send the input message to release
        requests_to_release.push(std::move(message));
        requests_on_the_fly.erase(request_id);
    }
}


// message gc
void message_gc() {
    log("GC-MSG", "Message GC thread has successfully started!");
    while (true) {
        auto messages = requests_to_release.pop_all();
        if (messages.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        // release the tensors
        for (auto &message: messages) {
            message->buffer_msg.rebuild();
        }
    }
}


// tensor gc
void tensor_gc() {
    log("GC-Tensor", "Tensor GC thread has successfully started!");
    std::deque<std::shared_ptr<torch::Tensor>> output_tensors_on_hold;
    std::deque<long> hold_time;

    while (true) {
        // put new tensors into the hold queue
        auto tensors = tensors_to_release.pop_all();
        for (auto &tensor: tensors) {
            output_tensors_on_hold.emplace_back(std::move(tensor));
            hold_time.emplace_back(get_time());
        }

        // remove tensors that are too old
        auto current_time = get_time();
        while (!hold_time.empty() && current_time - hold_time.front() > 10 * 1000 * 1000) {
            output_tensors_on_hold.pop_front();
            hold_time.pop_front();
            // log("GC-Tensor", "Cleaned up a tensor!");
        }

        // sleep for a while
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}


// sender
void sender_thread(const std::string &worker_ip) {
    log("Sender", "Sender thread has successfully started!");

    // wait until machine configs are available
    while (!machine_configs_initialized) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // get the machine config for the current worker
    Machine current_machine;
    for (const auto &machine: machine_configs) {
        if (machine.ip_address == worker_ip) {
            current_machine = machine;
            break;
        }
    }
    Assert(current_machine.machine_id != -1, "Could not find config for worker!");

    // get output machine id and ip pairs
    std::vector<std::pair<int, std::string>> output_id_ip;
    for (int machine_id: current_machine.out_nodes) {
        for (const auto &machine: machine_configs) {
            if (machine.machine_id == machine_id) {
                output_id_ip.emplace_back(machine_id, machine.ip_address);
            }
        }
    }
    for (const auto &id_ip: output_id_ip) {
        log("Sender", "Output machine: id=[" + std::to_string(id_ip.first) + "], ip=[" + id_ip.second + "]");
    }

    // initial the output sockets
    std::unordered_map<int, std::unique_ptr<PollServer>> output_sockets;
    for (const auto &id_ip: output_id_ip) {
        std::string bind_address = "tcp://" + worker_ip + ":" + std::to_string(BASE_PORT + id_ip.first);
        output_sockets[id_ip.first] = std::make_unique<PollServer>(context, bind_address);
    }

    // initialization
    // wait until the input ports are initialized (i.e. received Init message from all input machines)
    while (!input_port_initialized) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    log("Sender", "Start pushing init messages to the next stage!");
    // build message that can be reused
    Header init_header = Header();
    init_header.msg_type = MsgType::Init;
    init_header.creation_time = get_time();
    std::string init_msg_str = std::to_string(current_machine.machine_id);  // send the machine id out
    // send init until we receive the signal
    // we need to send repeatedly because some messages may be lost
    Assert(!stop_sending_init, "Stop sending init signal should not be true!");
    while (!stop_sending_init) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        for (const auto &id_ip: output_id_ip) {
            zmq::message_t init_msg(init_msg_str.data(), init_msg_str.size());
            output_sockets[id_ip.first]->send(init_header, init_msg);
        }
    }
    log("Sender", "Stop sending init messages to the next stage!");
    // send out one end init message to each output machine
    // this message will not be lost
    Header end_init_header = Header();
    end_init_header.msg_type = MsgType::InitComplete;
    end_init_header.creation_time = get_time();
    std::string end_init_msg_str = std::to_string(current_machine.machine_id);  // send the machine id out
    for (const auto &id_ip: output_id_ip) {
        zmq::message_t end_init_msg(end_init_msg_str.data(), end_init_msg_str.size());
        output_sockets[id_ip.first]->send(end_init_header, end_init_msg);
    }
    log("Sender", "Successfully finished initialization, entering main loop!");
    sender_in_main_loop = true;

    if (scheduler_type == "maxflow") {
        while (true) {
            // get all messages
            std::vector<MessageData> new_messages = compute_send_queue.pop_all();

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
        // initialize the swarm scheduler
        std::vector<int> out_ids;
        for (const auto &id_ip: output_id_ip) {
            out_ids.push_back(id_ip.first);
        }
        SwarmScheduler swarm_scheduler = SwarmScheduler(out_ids, 0.05, 0.8);

        // run the main loop
        while (true) {
            // get all messages
            std::vector<MessageData> new_messages = compute_send_queue.pop_all();

            for (auto &message: new_messages) {
                if (message.header.msg_type == MsgType::Prompt) {
                    // for prompt phase, we need swarm scheduler to tell us a route
                    int next_server_id = swarm_scheduler.choose_server();

                    // set next server id into the header
                    // We will decide the message's inference layers when it arrives at next node
                    message.header.add_stage(next_server_id, -1, -1);

                    // calculating delta time for the finished stage
                    Assert(message.header.current_stage >= 1, "Bad current stage!");
                    auto last_stage_start = message.header.swarm_delta[message.header.current_stage - 1];
                    message.header.swarm_delta[message.header.current_stage - 1] =
                            get_time() - last_stage_start;
                    Assert(message.header.swarm_delta[message.header.current_stage - 1] >= 0, "Bad delta time!");
                    // set time for swarm info field (will be used at next stage)
                    message.header.swarm_delta[message.header.current_stage] = get_time();

                    // send out the message
                    output_sockets[next_server_id]->send(message.header, message.buffer_msg);
                } else if (message.header.msg_type == MsgType::Decode) {
                    // for decode phase, just follow the route
                    int current_stage = message.header.current_stage;
                    int next_server_id = message.header.server_id[current_stage];

                    // calculating delta time for the finished stage
                    Assert(message.header.current_stage >= 1, "Bad current stage!");
                    auto last_stage_start = message.header.swarm_delta[message.header.current_stage - 1];
                    message.header.swarm_delta[message.header.current_stage - 1] =
                            get_time() - last_stage_start;
                    Assert(message.header.swarm_delta[message.header.current_stage - 1] >= 0, "Bad delta time!");
                    // set time for swarm info field (will be used at next stage)
                    message.header.swarm_delta[message.header.current_stage] = get_time();

                    // send the request following the route
                    output_sockets[next_server_id]->send(message.header, message.buffer_msg);
                } else if (message.header.msg_type == MsgType::SwarmInfo) {
                    // last layer only outputs to the host, no need to update
                    if (!is_last_layer) {
                        // update swarm scheduler
                        int current_stage = message.header.current_stage;
                        float delta_time = static_cast<float>(message.header.swarm_delta[current_stage]) / 1000000;
                        int next_server_id = message.header.server_id[current_stage];
                        swarm_scheduler.update_weights(next_server_id, delta_time);

                        // send the request following the route
                        output_sockets[next_server_id]->send(message.header, message.buffer_msg);
                    }
                } else if (message.header.msg_type == MsgType::Terminate) {
                    return;
                } else {
                    Assert(false, "Bad message type: " + std::to_string((int) message.header.msg_type));
                }
            }
        }
    } else if (scheduler_type == "random") {
        // get all output ids
        std::vector<int> out_ids;
        for (const auto &id_ip: output_id_ip) {
            out_ids.push_back(id_ip.first);
        }
        std::mt19937 gen(0);
        std::uniform_int_distribution<> distrib(0, (int) out_ids.size() - 1);

        // run the main loop
        while (true) {
            // get all messages
            std::vector<MessageData> new_messages = compute_send_queue.pop_all();

            for (auto &message: new_messages) {
                if (message.header.msg_type == MsgType::Prompt) {
                    // for prompt phase, we need to generate a random route
                    int random_index = distrib(gen);
                    int next_server_id = out_ids[random_index];

                    // set next server id into the header
                    // We will decide the message's inference layers when it arrives at next node
                    message.header.add_stage(next_server_id, -1, -1);

                    // send out the message
                    output_sockets[next_server_id]->send(message.header, message.buffer_msg);
                } else if (message.header.msg_type == MsgType::Decode) {
                    // for decode phase, just follow the route
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
    } else {
        Assert(false, "Bad scheduler type!");
    }
}


void worker_start_network_threads(const std::string &config_broadcast_addr, const std::string &worker_ip,
                                  const std::string &scheduler) {
    // config_broadcast_addr format: "tcp://10.128.0.53:5000"
    // worker_ip format: "10.128.0.53"
    // scheduler is: (1) maxflow, (2) swarm, (3) random
    std::cout << "Config broadcast address: " << config_broadcast_addr << std::endl;
    std::cout << "Worker IP address (local): " << worker_ip << std::endl;

    // check that network is not initialized
    Assert(!network_initialized, "Network threads have already been initialized!");
    network_initialized = true;

    // set scheduler
    Assert(scheduler == "swarm" || scheduler == "maxflow" || scheduler == "random", "Bad scheduler type");
    scheduler_type = scheduler;

    // creating threads
    std::thread t1(receiver_thread, config_broadcast_addr, worker_ip);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    std::thread t2(sender_thread, worker_ip);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    std::thread gc_thread1(message_gc);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    std::thread gc_thread2(tensor_gc);

    // detach from the main threads to avoid crash
    t1.detach();
    t2.detach();
    gc_thread1.detach();
    gc_thread2.detach();

    // wait until sender and receiver are initialized
    while (!receiver_in_main_loop || !sender_in_main_loop) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}


#endif //ZMQ_COMM_COMPUTE_WORKER_H
