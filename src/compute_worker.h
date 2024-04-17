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
        Header header = Header();
        header.creation_time = get_time();
        header.add_stage(1, 0, 2);
        header.add_stage(2, 2, 4);
        header.current_stage = counter++;
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
        if (!messages.empty() && DEBUG) {
            auto cur_time = get_time();
            log("Sender", "Received " + std::to_string(messages.size()) + " messages!");
            for (const auto &message: messages) {
                auto delta = cur_time - message.header.creation_time;
                log("Sender", "Comm time: " + std::to_string(delta) + " us " + "counter: " + std::to_string(message.header.current_stage));
            }
            Assert(last_counter + 1 == messages[0].header.current_stage, "Counter not increasing!");
            last_counter = messages.back().header.current_stage;
        }
        // END TODO
    }
}

void run_compute_worker() {
    // initialize zmq context
    zmq::context_t context(1);

    // Creating three threads
    std::thread t1(receiver_thread, std::ref(context));
    std::thread t2(compute_thread, std::ref(context));
    std::thread t3(sender_thread, std::ref(context));

    // Joining all threads with the main thread
    t1.join();
    t2.join();
    t3.join();
}


#endif //ZMQ_COMM_COMPUTE_WORKER_H
