//
// Created by meiyixuan on 2024/4/15.
//
#include "../src/poller.h"
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
    int msg_id = 0;
    while (true) {
        // construct message
        std::string cur_msg = "Hello from server " + address + ", msg id " + std::to_string(msg_id++);
        std::string time_msg = std::to_string(get_time());
        server.send(cur_msg, time_msg);

        // Sleep for demonstration purposes
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

}