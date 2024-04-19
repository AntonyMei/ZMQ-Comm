//
// Created by meiyixuan on 2024/4/19.
//
#include "../src/config_parser.h"

int main() {
    std::vector<Machine> machines = read_config("../tests/test_config.txt");

    // Display the data for verification
    for (const auto &m: machines) {
        std::cout << "Machine ID: [" << m.machine_id << "], IP: [" << m.ip_address << "]" << std::endl;
        std::cout << "Start Layer: " << m.start_layer << ", End Layer: " << m.end_layer << std::endl;
        std::cout << "In Nodes: ";
        for (int in: m.in_nodes) std::cout << in << " ";
        std::cout << std::endl << "Out Nodes: ";
        for (int out: m.out_nodes) std::cout << out << " ";
        std::cout << std::endl << std::endl;
    }

    // test serialization and deserialization
    for (const auto &m: machines) {
        std::string serialized = serialize_machine(m);
        Machine deserialized = deserialize_machine(serialized);
        print_machine(deserialized);
    }

    return 0;
}
