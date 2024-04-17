//
// Created by meiyixuan on 2024/4/17.
//

#ifndef ZMQ_COMM_INPROC_QUEUE_H
#define ZMQ_COMM_INPROC_QUEUE_H

#include <iostream>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <cstring>
#include <zmq.hpp>
#include <thread>


struct MessageData {
    Header header;
    zmq::message_t buffer_msg;

    MessageData(Header hdr, zmq::message_t &&buf) : header(hdr), buffer_msg(std::move(buf)) {}
};

template<typename T>
class ThreadSafeQueue {
private:
    std::queue<T> queue;
    std::mutex mutex;

public:
    void push(T &&value) {
        std::lock_guard<std::mutex> lock(mutex);
        queue.push(std::move(value));
    }

    std::vector<T> pop_all() {
        std::unique_lock<std::mutex> lock(mutex);
        std::vector<T> result;
        while (!queue.empty()) {
            result.emplace_back(std::move(queue.front()));
            queue.pop();
        }
        return std::move(result);
    }
};


#endif //ZMQ_COMM_INPROC_QUEUE_H
