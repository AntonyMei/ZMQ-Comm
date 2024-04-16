//
// Created by meiyixuan on 2024/4/15.
//
#include "../../src/poller.h"
#include <thread>

int main(int argc, char *argv[]) {
    // get the port to work on
    if (argc < 2) {
        std::cerr << "Require port id on server!\n";
        return 1;
    }
    std::string ip_str = argv[1];
    std::string port_str = argv[2];
    auto address = "tcp://" + ip_str + ":" + port_str;

    // initialize context and polling server
    zmq::context_t context(1);
    PollServer server = PollServer(context, address);

    // send out messages
    constexpr size_t buffer_size = 16 * 1024;
    std::vector<char> buffer(buffer_size, 'a');
    for (int msg_id = 0; msg_id < 100 * 1000; ++msg_id) {
        // build header and buffer
        Header header = Header(MsgType::Prompt);
        header.add_stage(1, 0, 2);
        header.add_stage(2, 2, 4);
        zmq::message_t buffer_msg(buffer.data(), buffer.size());

        // send through zmq
        server.send(header, buffer_msg);
    }
}