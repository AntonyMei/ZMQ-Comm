//
// Created by meiyixuan on 2024/4/21.
//

#include "../src/swarm.h"
#include <iostream>


int main() {
    auto swarm_scheduler = SwarmScheduler(std::vector<int>({1, 2, 3, 4}), 1, 1);
    for (int i = 0; i < 12; ++i) {
        std::cout << swarm_scheduler.choose_server() << std::endl;
    }
    std::cout << "Update weights\n";
    swarm_scheduler.update_weights(1, 0.5);
    for (int i = 0; i < 10; ++i) {
        std::cout << swarm_scheduler.choose_server() << std::endl;
    }
}