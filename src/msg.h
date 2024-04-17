//
// Created by meiyixuan on 2024/4/15.
//

#ifndef ZMQ_COMM_MSG_H
#define ZMQ_COMM_MSG_H

#include <zmq.hpp>
#include <vector>
#include <cstring>

#include "utils.h"
#include "const.h"


enum class MsgType {
    Invalid = 0,
    // cluster initialization
    Init = 1,
    InitComplete = 1,
    // running tasks
    Prompt = 2,
    Decode = 3,
    // info
    SwarmInfo = 4,
    // termination
    Terminate = 5,
};


class __attribute__((packed)) Header {
public:
    Header() = default;

    void add_stage(int _server_id, int _start_layer_idx, int _end_layer_idx) {
        Assert(total_stages < MAX_HOP, "Too many stages!");
        server_id[total_stages] = _server_id;
        start_layer_idx[total_stages] = _start_layer_idx;
        end_layer_idx[total_stages] = _end_layer_idx;
        total_stages += 1;
    }

    zmq::message_t serialize() const {
        zmq::message_t message(sizeof(Header));
        std::memcpy(message.data(), this, sizeof(Header));
        return message;
    }

    static Header deserialize(const zmq::message_t &message) {
        Header header;
        std::memcpy(&header, message.data(), sizeof(Header));
        return header;
    }

public:
    // msg type
    MsgType msg_type = MsgType::Invalid;
    long creation_time = 0;
    // routing data
    int current_stage = 0;
    int total_stages = 0;
    int server_id[MAX_HOP]{};
    int start_layer_idx[MAX_HOP]{};
    int end_layer_idx[MAX_HOP]{};
    // for info message only
    float swarm_delta[MAX_HOP]{};
};

Header generate_random_header() {
    Header header;
    header.msg_type = MsgType::Prompt;
    header.creation_time = get_time();
    // add random stages
    int total_stages = rand() % MAX_HOP;
    for (int i = 0; i < total_stages; i++) {
        header.add_stage(rand() % 10, rand() % 10, rand() % 10);
        header.swarm_delta[i] = rand() % 10;
    }
    return header;
}

void print_header(const std::string& logger, const Header& header) {
    std::cout << "[" << logger << "]\n";
    std::cout << "msg_type: " << static_cast<int>(header.msg_type) << "\n";
    std::cout << "creation_time: " << header.creation_time << "\n";
    std::cout << "current_stage: " << header.current_stage << "\n";
    std::cout << "total_stages: " << header.total_stages << "\n";
    for (int i = 0; i < header.total_stages; i++) {
        std::cout << "server_id[" << i << "]: " << header.server_id[i] << ", ";
        std::cout << "start_layer_idx[" << i << "]: " << header.start_layer_idx[i] << ", ";
        std::cout << "end_layer_idx[" << i << "]: " << header.end_layer_idx[i] << ", ";
        std::cout << "swarm_delta[" << i << "]: " << header.swarm_delta[i] << "\n";
    }
    std::cout << std::endl;
}

#endif //ZMQ_COMM_MSG_H
