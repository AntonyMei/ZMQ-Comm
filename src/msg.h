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
    InitComplete = 2,
    // running tasks
    Prompt = 3,
    Decode = 4,
    // info
    SwarmInfo = 5,
    // termination
    Terminate = 6,
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

    [[nodiscard]] zmq::message_t serialize() const {
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
    // basic message info
    MsgType msg_type = MsgType::Invalid;
    long creation_time = 0;
    int request_id = 0;
    // request info
    int num_tokens = 0;   // for prompt = token number, for decode = context size
    int max_tokens = 0;   // max number of tokens to generate, when reaching this we will force cut
    // routing data
    int current_stage = 0;
    int total_stages = 0;
    int server_id[MAX_HOP]{};
    int start_layer_idx[MAX_HOP]{};
    int end_layer_idx[MAX_HOP]{};
    // for swarm only (unit is us)
    long swarm_delta[MAX_HOP]{};
};

Header generate_random_header() {
    Header header;
    header.msg_type = MsgType::Prompt;
    header.creation_time = get_time();
    header.request_id = rand() % 100;
    header.num_tokens = rand() % 100;
    header.max_tokens = header.num_tokens + rand() % 100;
    // add random stages
    int total_stages = rand() % (MAX_HOP - 1) + 1;
    for (int i = 0; i < total_stages; i++) {
        header.add_stage(rand() % 10, i, i + 1);
        header.swarm_delta[i] = rand() % 10;
    }
    return header;
}

void print_header(const std::string& logger, const Header& header) {
    std::cout << "[" << logger << "]\n";
    std::cout << "msg_type: " << static_cast<int>(header.msg_type) << "\n";
    std::cout << "creation_time: " << header.creation_time << "\n";
    std::cout << "request_id: " << header.request_id << "\n";
    std::cout << "num_tokens: " << header.num_tokens << "\n";
    std::cout << "max_tokens: " << header.max_tokens << "\n";
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
