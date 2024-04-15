//
// Created by meiyixuan on 2024/4/15.
//

#ifndef ZMQ_COMM_UTILS_H
#define ZMQ_COMM_UTILS_H

#include <iostream>
#include <string>
#include <assert.h>

void Assert(bool cond, std::string err_msg) {
    if (!cond) {
        std::cerr << err_msg << std::endl;
        assert(false);
    }
}

#endif //ZMQ_COMM_UTILS_H
