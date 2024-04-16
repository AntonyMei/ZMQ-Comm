//
// Created by meiyixuan on 2024/4/15.
//

#ifndef ZMQ_COMM_MSG_H
#define ZMQ_COMM_MSG_H

#include <zmq.hpp>
#include <vector>
#include <cstring>

#include "utils.h"

#define MAX_HOP 16

enum class MsgType {
    Invalid = 0,
    // cluster initialization
    Init = 1,
    InitComplete = 1,
    // running tasks
    Prompt = 2,
    Decode = 3,
};


class __attribute__((packed)) Header {
public:
    Header() = default;

    explicit Header(MsgType _msg_type) {
        msg_type = _msg_type;
        creation_time = get_time();
    }

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
};

#endif //ZMQ_COMM_MSG_H
