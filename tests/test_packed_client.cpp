//
// Created by meiyixuan on 2024/4/15.
//
#include "../src/poller.h"

int main() {
    // ports and context
    std::vector<std::string> server_addresses = {
            "tcp://10.128.0.42:5555",
            "tcp://10.128.0.43:5555",
    };

    // initialize polling client
    zmq::context_t context(1);
    PollingClient client = PollingClient(context, server_addresses);
    std::cout << "client initialized!" << std::endl;
    while (true) {
        client.poll_once(1);
    }
}
