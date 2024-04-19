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

#include "utils.h"
#include "const.h"
#include "msg.h"
#include "inproc_queue.h"

// global queue
bool network_initialized = false;
ThreadSafeQueue<MessageData> recv_compute_queue;
ThreadSafeQueue<MessageData> compute_send_queue;

// the requests that are in compute
std::unordered_map<int, std::shared_ptr<MessageData>> requests_on_the_fly;


// receiver
void receiver_thread(zmq::context_t &_context) {
    // initialize
    log("Receiver", "Receiver thread has successfully started!");

    int counter = 0;
    while (true) {
        // TODO: network receiver (now we use some dummy workload to simulate the network receiver)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        constexpr size_t buffer_size = 8192 * 2 * 1000;
        std::vector<char> buffer(buffer_size, 'a');
        zmq::message_t buffer_msg(buffer.data(), buffer.size());   // there is a copy here
        Header header = generate_random_header();
        header.request_id = counter++;
        if (counter == 33) {
            break;
        }
        // END TODO

        // send the header and buffer to compute thread
        zmq::message_t header_msg = header.serialize();
        recv_compute_queue.push(MessageData(header, std::move(buffer_msg)));
    }
}


// compute - step 1: fetch
// Return values:
//  1. request_ids: List[int]
//  2. is_prompt: List[bool]
//  3. start_layer_idx: List[int]
//  4. end_layer_idx: List[int]
//  5. num_tokens: List[int]
//  6. max_tokens: List[int]
//  7. tensors: List[Torch.Tensor]
std::tuple<std::vector<int>, std::vector<bool>, std::vector<int>, std::vector<int>, std::vector<int>,
        std::vector<int>, std::vector<torch::Tensor>> fetch_new_requests() {
    // TODO: process special messages
    // read all messages from the receiver
    std::vector<MessageData> messages = recv_compute_queue.pop_all();

    // process the messages into torch tensors
    std::vector<torch::Tensor> tensors;
    auto options = torch::TensorOptions().dtype(torch::kFloat16);
    for (auto &message: messages) {
        auto *data = static_cast<c10::Half *>(message.buffer_msg.data());
        auto length = static_cast<int>(message.buffer_msg.size() / 2);
        torch::Tensor tensor = torch::from_blob(data, {length}, options);
        tensors.emplace_back(std::move(tensor));
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

    // return the tensors
    return {std::move(request_ids), std::move(is_prompt), std::move(start_layer_idx), std::move(end_layer_idx),
            std::move(num_tokens), std::move(max_tokens), std::move(tensors)};
}


// compute
void compute_thread(zmq::context_t &_context) {
    // initialize
    log("Compute", "Compute thread has successfully started!");

    while (true) {
        // receive all messages from the network receiver thread
        std::vector<MessageData> messages = recv_compute_queue.pop_all();

        // TODO: compute (now we use some dummy workload to simulate the compute)
        std::vector<MessageData> new_messages = std::move(messages);
        // END TODO

        // send the processed message to the network sender thread
        for (auto &message: new_messages) {
            compute_send_queue.push(std::move(message));
        }
    }
}


// sender
void sender_thread(zmq::context_t &_context) {
    log("Sender", "Sender thread has successfully started!");

    int last_counter = -1;
    while (true) {
        // receive all messages from the compute thread
        std::vector<MessageData> messages = compute_send_queue.pop_all();

        // TODO: network sender (now we use some dummy workload to simulate the network sender)
        if (DEBUG) {
            auto finish_time = get_time();
            for (const auto &message: messages) {
                auto delta_time = finish_time - message.header.creation_time;
                std::cout << "Delta time: " << delta_time << " us\n";
                print_header("Sender", message.header);
            }
        }
        // END TODO
    }
}


#endif //ZMQ_COMM_COMPUTE_WORKER_H
