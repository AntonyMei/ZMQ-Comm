//
// Created by meiyixuan on 2024/4/15.
//

#ifndef ZMQ_COMM_UTILS_H
#define ZMQ_COMM_UTILS_H

#include <iostream>
#include <string>
#include <cassert>

void Assert(bool cond, const std::string& err_msg) {
    if (!cond) {
        std::cerr << err_msg << std::endl;
        assert(false);
    }
}

long get_time() {
    // results in micro-seconds
    auto now = std::chrono::system_clock::now();
    auto epoch = now.time_since_epoch();
    auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(epoch).count();
    return microseconds;
}

#endif //ZMQ_COMM_UTILS_H
