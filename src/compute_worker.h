//
// Created by meiyixuan on 2024/4/17.
//

#ifndef ZMQ_COMM_COMPUTE_WORKER_H
#define ZMQ_COMM_COMPUTE_WORKER_H

#include <iostream>
#include <thread>
#include <zmq.hpp>

#include "utils.h"
#include "const.h"
#include "msg.h"
#include "inproc_queue.h"


ThreadSafeQueue<MessageData> recv_compute_queue;
ThreadSafeQueue<MessageData> compute_send_queue;


// receiver
void receiver_thread(zmq::context_t &_context) {
    // initialize
    log("Receiver", "Receiver thread has successfully started!");

    int counter = 0;
    while (true) {
        // TODO: network receiver (now we use some dummy workload to simulate the network receiver)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        constexpr size_t buffer_size = 1600 * 1024;
        std::vector<char> buffer(buffer_size, 'a');
        zmq::message_t buffer_msg(buffer.data(), buffer.size());   // there is a copy here
        Header header = generate_random_header();
        header.current_stage = counter++;
        if (DEBUG) {
            print_header("Receiver", header);
        }
        // END TODO

        // send the header and buffer to compute thread
        zmq::message_t header_msg = header.serialize();
        recv_compute_queue.push(MessageData(header, std::move(buffer_msg)));
    }
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
