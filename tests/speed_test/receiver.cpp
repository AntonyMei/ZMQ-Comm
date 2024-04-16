//
// Created by meiyixuan on 2024/4/15.
//
#include "../../src/poller.h"

int main() {
    // ports and context
    std::vector<std::string> server_addresses = {
            "tcp://10.128.0.42:5555",
            "tcp://10.128.0.43:5555",
    };

    // initialize polling client
    zmq::context_t context(1);
    PollingClient client = PollingClient(context, server_addresses);
    while (true) {
        // poll to get message
        zmq::message_t buffer_msg;
        Header header = client.poll_once(buffer_msg, 10);
        auto receive_time = get_time();

        // check and print
        if (header.msg_type != MsgType::Invalid) {
            // std::string text(static_cast<char*>(buffer_msg.data()), buffer_msg.size());
            // std::cout << "Received: " << text << std::endl;
            std::cout << "Received: " << std::endl;
            std::cout << "ser - send - recv - des: " << receive_time - header.creation_time << " us\n";
            std::cout << header.creation_time << std::endl;
            std::cout << static_cast<int>(header.msg_type) << std::endl;
            std::cout << header.current_stage << std::endl;
            std::cout << header.total_stages << std::endl;
            for (int i = 0; i < header.total_stages; i++) {
                std::cout << header.server_id[i] << " " << header.start_layer_idx[i] << " " << header.end_layer_idx[i]
                          << std::endl;
            }
            std::cout << buffer_msg.size() << std::endl;
        }
    }
}
//
// Created by meiyixuan on 2024/4/15.
//
