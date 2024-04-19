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

// flying batch
bool exist_flying_batch = false;
std::vector<MessageData> flying_batch;


// receiver
void receiver_thread(zmq::context_t &_context) {
    // initialize
    log("Receiver", "Receiver thread has successfully started!");

    int counter = 0;
    while (true) {
        // TODO: network receiver (now we use some dummy workload to simulate the network receiver)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        constexpr size_t buffer_size = 8192 * 2;
        std::vector<char> buffer(buffer_size, 'a');
        zmq::message_t buffer_msg(buffer.data(), buffer.size());   // there is a copy here
        Header header = generate_random_header();
        header.current_stage = counter++;
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
// Return value: List[Torch.Tensor]
std::vector<torch::Tensor> fetch_new_requests() {
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

    // save the flying batch
    Assert(!exist_flying_batch, "There is already a flying batch!");
    if (!messages.empty()) {
        flying_batch = std::move(messages);
        exist_flying_batch = true;
    }

    // return the tensors
    return std::move(tensors);
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
