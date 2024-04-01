//
// Created by meiyixuan on 2024/4/1.
//
#include <zmq.hpp>
#include <iostream>
#include <thread>

int main(int argc, char *argv[]) {
    // get the port to work on
    if (argc < 2) {
        std::cerr << "Require port id on server!\n";
        return 1;
    }
    std::string port_str = argv[1];

    // prepare context and socket
    zmq::context_t context(1);
    zmq::socket_t publisher(context, ZMQ_PUB);
    publisher.bind("tcp://*:" + port_str);
    std::cout << "This server is working on address: " << "tcp://*:" + port_str << "\n";

    // send messages
    int msg_id = 0;
    while (true) {
        // construct message
        std::string cur_msg = "Hello from server " + port_str + ", msg id " + std::to_string(msg_id++);
        zmq::message_t message(cur_msg.data(), cur_msg.size());
        publisher.send(message, zmq::send_flags::none);

        // Sleep for demonstration purposes
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
