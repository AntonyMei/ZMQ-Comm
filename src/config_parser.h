//
// Created by meiyixuan on 2024/4/19.
//

#ifndef ZMQ_COMM_CONFIG_PARSER_H
#define ZMQ_COMM_CONFIG_PARSER_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cctype>
#include <locale>

struct Machine {
    int machine_id = -1;
    std::string ip_address;
    std::vector<int> in_nodes;
    std::vector<int> out_nodes;
    int start_layer = -1;
    int end_layer = -1;
};

// Trim from start (in place)
static inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
}

// Trim from end (in place)
static inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

// Trim from both ends (in place)
static inline void trim(std::string &s) {
    ltrim(s);
    rtrim(s);
}

void print_machine(const Machine &m) {
    std::cout << "Machine ID: [" << m.machine_id << "], IP: [" << m.ip_address << "]" << std::endl;
    std::cout << "Start Layer: " << m.start_layer << ", End Layer: " << m.end_layer << std::endl;
    std::cout << "In Nodes: ";
    for (int in: m.in_nodes) std::cout << in << " ";
    std::cout << std::endl << "Out Nodes: ";
    for (int out: m.out_nodes) std::cout << out << " ";
    std::cout << std::endl << std::endl;
}

std::vector<Machine> read_config(const std::string &filename) {
    std::vector<Machine> machines;
    std::ifstream file(filename);
    std::string line;

    Machine machine;

    if (!file.is_open()) {
        std::cerr << "Failed to open file." << std::endl;
        return machines;
    }

    while (getline(file, line)) {
        std::istringstream iss(line);
        std::string key, val;
        getline(iss, key, ':');
        getline(iss, val);
        trim(val);

        if (key.find("machine_id") != std::string::npos) {
            if (machine.machine_id != -1) {  // Save the previous machine if there is one
                machines.push_back(machine);
                machine = Machine();  // Reset the machine struct for new data
            }
            machine.machine_id = std::stoi(val);
        } else if (key.find("ip_address") != std::string::npos) {
            machine.ip_address = val;
        } else if (key.find("in_nodes") != std::string::npos) {
            std::istringstream ss(val);
            int id;
            while (ss >> id) {
                machine.in_nodes.push_back(id);
                if (ss.peek() == ',') ss.ignore();
            }
        } else if (key.find("out_nodes") != std::string::npos) {
            std::istringstream ss(val);
            int id;
            while (ss >> id) {
                machine.out_nodes.push_back(id);
                if (ss.peek() == ',') ss.ignore();
            }
        } else if (key.find("start_layer") != std::string::npos) {
            machine.start_layer = std::stoi(val);
        } else if (key.find("end_layer") != std::string::npos) {
            machine.end_layer = std::stoi(val);
        }
    }

    if (machine.machine_id != 0) {  // Save the last machine
        machines.push_back(machine);
    }

    return machines;
}

// Serialize a Machine object to a binary stream
std::string serialize_machine(const Machine& machine) {
    std::ostringstream oss(std::ios::binary);

    // Serialize machine_id
    oss.write(reinterpret_cast<const char*>(&machine.machine_id), sizeof(machine.machine_id));
    oss.write(reinterpret_cast<const char*>(&machine.start_layer), sizeof(machine.start_layer));
    oss.write(reinterpret_cast<const char*>(&machine.end_layer), sizeof(machine.end_layer));

    // Serialize ip_address
    size_t ip_length = machine.ip_address.size();
    oss.write(reinterpret_cast<const char*>(&ip_length), sizeof(ip_length));
    oss.write(machine.ip_address.data(), (int)ip_length);

    // Helper function to serialize vector of ints
    auto serialize_vector = [&oss](const std::vector<int>& vec) {
        size_t vec_size = vec.size();
        oss.write(reinterpret_cast<const char*>(&vec_size), sizeof(vec_size));
        for (int value : vec) {
            oss.write(reinterpret_cast<const char*>(&value), sizeof(value));
        }
    };

    // Serialize in_nodes and out_nodes
    serialize_vector(machine.in_nodes);
    serialize_vector(machine.out_nodes);

    return oss.str();
}

// Deserialize a Machine object from a binary stream
Machine deserialize_machine(const std::string& data) {
    std::istringstream iss(data, std::ios::binary);
    Machine machine;

    // Deserialize machine_id
    iss.read(reinterpret_cast<char*>(&machine.machine_id), sizeof(machine.machine_id));
    iss.read(reinterpret_cast<char*>(&machine.start_layer), sizeof(machine.start_layer));
    iss.read(reinterpret_cast<char*>(&machine.end_layer), sizeof(machine.end_layer));

    // Deserialize ip_address
    size_t ip_length;
    iss.read(reinterpret_cast<char*>(&ip_length), sizeof(ip_length));
    machine.ip_address.resize(ip_length);
    iss.read(&machine.ip_address[0], (int)ip_length);

    // Helper function to deserialize vector of ints
    auto deserialize_vector = [&iss](std::vector<int>& vec) {
        size_t vec_size;
        iss.read(reinterpret_cast<char*>(&vec_size), sizeof(vec_size));
        vec.resize(vec_size);
        for (size_t i = 0; i < vec_size; ++i) {
            iss.read(reinterpret_cast<char*>(&vec[i]), sizeof(int));
        }
    };

    // Deserialize in_nodes and out_nodes
    deserialize_vector(machine.in_nodes);
    deserialize_vector(machine.out_nodes);

    return machine;
}

std::string serialize_vector_of_machines(const std::vector<Machine>& machines) {
    std::ostringstream oss(std::ios::binary);

    // Serialize the size of the vector
    size_t num_machines = machines.size();
    oss.write(reinterpret_cast<const char*>(&num_machines), sizeof(num_machines));

    // Serialize each machine
    for (const auto& machine : machines) {
        std::string machine_data = serialize_machine(machine);
        size_t machine_size = machine_data.size();
        oss.write(reinterpret_cast<const char*>(&machine_size), sizeof(machine_size));
        oss.write(machine_data.data(), (int)machine_size);
    }

    return oss.str();
}

std::vector<Machine> deserialize_vector_of_machines(const std::string& data) {
    std::istringstream iss(data, std::ios::binary);
    std::vector<Machine> machines;

    // Deserialize the size of the vector
    size_t num_machines;
    iss.read(reinterpret_cast<char*>(&num_machines), sizeof(num_machines));

    // Deserialize each machine
    for (size_t i = 0; i < num_machines; ++i) {
        size_t machine_size;
        iss.read(reinterpret_cast<char*>(&machine_size), sizeof(machine_size));
        std::vector<char> machine_data(machine_size);
        iss.read(machine_data.data(), (int)machine_size);
        machines.push_back(deserialize_machine(std::string(machine_data.begin(), machine_data.end())));
    }

    return machines;
}



#endif //ZMQ_COMM_CONFIG_PARSER_H
