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

// gc queues
ThreadSafeQueue<std::shared_ptr<MessageData>> requests_to_release;
ThreadSafeQueue<std::shared_ptr<torch::Tensor>> tensors_to_release;


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
        if (counter == 128) {
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


// Compute - step 2: submit
// Args:
//  1. request_ids: List[int]
//  2. offsets: List[int] (in number of fp16 elements)
//  3. lengths: List[int] (in number of fp16 elements)
//  4. result_tensor: Torch.Tensor (a cpu tensor)
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

        // build new zmq message
        auto* start_ptr = result_tensor.data_ptr<c10::Half>() + offset;
        zmq::message_t new_buf_msg(start_ptr, length * result_tensor.element_size(), custom_free, nullptr);
        auto new_message = MessageData(message->header, std::move(new_buf_msg));

        // send the message to the compute thread
        compute_send_queue.push(std::move(new_message));

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
            log("GC-Tensor", "Cleaned up a tensor!");
        }

        // sleep for a while
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}


// sender
void sender_thread(zmq::context_t &_context) {
    log("Sender", "Sender thread has successfully started!");

    while (true) {
        // receive all messages from the compute thread
        std::vector<MessageData> messages = compute_send_queue.pop_all();

        // TODO: network sender (now we use some dummy workload to simulate the network sender)
        if (DEBUG) {
            auto finish_time = get_time();
            for (const auto &message: messages) {
                auto delta_time = finish_time - message.header.creation_time;
                std::cout << "Request id: " << message.header.request_id << "\n";
                std::cout << "Delta time: " << delta_time << " us\n";
//                print_header("Sender", message.header);
            }
        }
        // END TODO
    }
}


#endif //ZMQ_COMM_COMPUTE_WORKER_H
