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
        }
    }

    if (machine.machine_id != 0) {  // Save the last machine
        machines.push_back(machine);
    }

    return machines;
}


#endif //ZMQ_COMM_CONFIG_PARSER_H
