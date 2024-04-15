//
// Created by meiyixuan on 2024/4/15.
//

#ifndef ZMQ_COMM_POLLER_H
#define ZMQ_COMM_POLLER_H

#include <utility>
#include <zmq.hpp>
#include <iostream>
#include <vector>

#include "utils.h"


class PollingClient {
public:
    PollingClient(zmq::context_t &_context, std::vector<std::string> &_server_addresses) : context(_context) {
        // initialize socket
        server_addresses = _server_addresses;
        subscriber = zmq::socket_t(context, ZMQ_SUB);
        for (const auto &addr: server_addresses) {
            subscriber.connect(addr);
            std::cout << "Receiver connected to address: " << addr << "\n";
        }
        subscriber.set(zmq::sockopt::subscribe, "");  // subscribe all messages

        // create a poller
        poller = zmq::poller_t<>();
        poller.add(subscriber, zmq::event_flags::pollin);
    }

    void poll_once(int time_out = 10) {
        // poll one message
        std::vector<zmq::poller_event<>> events(1);
        size_t rc = poller.wait_all(events, std::chrono::milliseconds(time_out));
        Assert(rc == 0 || rc == 1, "Bad receive count!");

        // process the result
        // TODO: change to real workload
        if (rc == 1) {
            zmq::message_t message;
            events[0].socket.recv(message, zmq::recv_flags::none);
            std::string text(static_cast<char *>(message.data()), message.size());
            std::cout << "Received: " << text << std::endl;
        }
    }

private:
    std::vector<std::string> server_addresses;
    zmq::context_t &context;
    zmq::socket_t subscriber;
    zmq::poller_t<> poller;
};


class PollServer {
public:
    PollServer(zmq::context_t &_context, std::string &bind_address) : context(_context) {
        // initialize socket
        publisher = zmq::socket_t(context, ZMQ_PUB);
        publisher.bind(bind_address);
        std::cout << "Sender bound to address: " << bind_address << "\n";
    }

    void send(std::string &msg) {
        // TODO: change to real workload
        zmq::message_t message(msg.data(), msg.size());
        publisher.send(message, zmq::send_flags::none);
    }

private:
    zmq::context_t &context;
    zmq::socket_t publisher;
};


#endif //ZMQ_COMM_POLLER_H
