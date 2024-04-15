//
// Created by meiyixuan on 2024/4/1.
//
#include <zmq.hpp>
#include <iostream>
#include <vector>

int main () {
    // ports
    std::vector<std::string> server_addresses = {
            "tcp://10.128.0.42:5555",
            "tcp://10.128.0.42:5555",
    };

    // create context and socket
    zmq::context_t context(1);
    zmq::socket_t subscriber(context, ZMQ_SUB);
    for (const auto& addr : server_addresses) {
        subscriber.connect(addr);
    }
    subscriber.set(zmq::sockopt::subscribe, "");  // subscribe all messages

    // create a poller
    zmq::poller_t<> poller;
    poller.add(subscriber, zmq::event_flags::pollin);

    // polling
    while (true) {
        std::vector<zmq::poller_event<>> events(1);
        int rc = poller.wait_all(events, std::chrono::milliseconds(1000)); // Adjust timeout as needed

        for (auto& event : events) {
            zmq::message_t message;
            event.socket.recv(message, zmq::recv_flags::none);
            std::string text(static_cast<char*>(message.data()), message.size());
            std::cout << "Received: " << text << std::endl;
        }
    }
}