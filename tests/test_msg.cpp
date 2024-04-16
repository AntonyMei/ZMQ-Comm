//
// Created by meiyixuan on 2024/4/15.
//

#include "../src/msg.h"
#include <thread>

int main() {
    // size check
    auto header = Header(MsgType::Invalid);
    std::cout << sizeof(Header) << std::endl;
    std::cout << sizeof(header) << std::endl;
    std::cout << sizeof(header.msg_type) << std::endl;
    std::cout << sizeof(header.creation_time) << std::endl;
    std::cout << sizeof(header.current_stage) << std::endl;
    std::cout << sizeof(header.total_stages) << std::endl;
    std::cout << sizeof(header.server_id) << std::endl;
    std::cout << sizeof(header.start_layer_idx) << std::endl;
    std::cout << sizeof(header.end_layer_idx) << std::endl;

    // serialization and deserialization time
    auto start = get_time();
    for (int i = 0; i < 1000 * 1000; i++) {
        auto message = header.serialize();
        auto new_header = Header::deserialize(message);
    }
    auto end = get_time();
    std::cout << "Start time: " << start << std::endl;
    std::cout << "End time: " << end << std::endl;
    std::cout << "Serialization and deserialization time (ns): " << (end - start) / 1000 << std::endl;

    // serialization and deserialization correctness
    auto test_header = Header(MsgType::Init);
    test_header.add_stage(1, 0, 2);
    test_header.add_stage(2, 2, 4);
    test_header.add_stage(3, 4, 6);
    auto test_message = test_header.serialize();
    auto new_test_header = Header::deserialize(test_message);
    std::cout << "Msg type: " << static_cast<int>(new_test_header.msg_type) << std::endl;
    std::cout << "Creation time: " << new_test_header.creation_time << std::endl;
    std::cout << "Current stage: " << new_test_header.current_stage << std::endl;
    std::cout << "Total stages: " << new_test_header.total_stages << std::endl;
    for (int i = 0; i < new_test_header.total_stages; i++) {
        std::cout << "Stage " << i << ": " << new_test_header.server_id[i] << " " << new_test_header.start_layer_idx[i]
                  << " " << new_test_header.end_layer_idx[i] << std::endl;
    }

    // check the correctness of the header
    Assert(new_test_header.msg_type == MsgType::Init, "Msg type incorrect!");
    Assert(new_test_header.creation_time == test_header.creation_time, "Creation time incorrect!");
    Assert(new_test_header.current_stage == 0, "Current stage incorrect!");
    Assert(new_test_header.total_stages == 3, "Total stages incorrect!");
    Assert(new_test_header.server_id[0] == 1, "Server id incorrect!");
    Assert(new_test_header.start_layer_idx[0] == 0, "Start layer idx incorrect!");
    Assert(new_test_header.end_layer_idx[0] == 2, "End layer idx incorrect!");
    Assert(new_test_header.server_id[1] == 2, "Server id incorrect!");
    Assert(new_test_header.start_layer_idx[1] == 2, "Start layer idx incorrect!");
    Assert(new_test_header.end_layer_idx[1] == 4, "End layer idx incorrect!");
    Assert(new_test_header.server_id[2] == 3, "Server id incorrect!");
    Assert(new_test_header.start_layer_idx[2] == 4, "Start layer idx incorrect!");
    Assert(new_test_header.end_layer_idx[2] == 6, "End layer idx incorrect!");

    // message build time
    constexpr size_t bufferSize = 16 * 1024;
    std::vector<char> buffer(bufferSize, 'a');
    start = get_time();
    for (int i = 0; i < 1000; i++) {
        zmq::message_t buffer_msg(buffer.data(), buffer.size());
    }
    end = get_time();
    std::cout << "Message build time (ns): " << end - start << std::endl;
}
