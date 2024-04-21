//
// Created by meiyixuan on 2024/4/21.
//

#ifndef ZMQ_COMM_SWARM_H
#define ZMQ_COMM_SWARM_H

#include <unordered_map>
#include <queue>


struct ComparePriority {
    bool operator()(std::pair<float, int> const& a, std::pair<float, int> const& b) {
        // Lower float value has higher priority (min-heap)
        // Elements with small weights will come out first.
        return a.first > b.first;
    }
};


class SwarmScheduler {
public:
    SwarmScheduler(const std::vector<int> &node_ids, float _initial_priority, float _smoothing_factor) {
        // save parameters
        initial_priority = _initial_priority;
        smoothing_factor = _smoothing_factor;

        // initialize ema and queue
        for (const int node_id: node_ids) {
            ema[node_id] = _initial_priority;
            queue.emplace(_initial_priority, node_id);
        }
    }

    int choose_server() {
        // choose one server
        auto chosen = queue.top();
        queue.pop();
        float priority = chosen.first;
        int node_id = chosen.second;

        // place it back into queue
        float new_priority = priority + ema[node_id];
        queue.emplace(new_priority, node_id);
        return node_id;
    }

    void update_weights(int node_id, float delta_t) {
        ema[node_id] = smoothing_factor * delta_t + (1 - smoothing_factor) * ema[node_id];
    }

public:
    // parameters
    float initial_priority;
    float smoothing_factor;

    // queues
    std::unordered_map<int, float> ema;  // machine id -> weights
    std::priority_queue<std::pair<float, int>, std::vector<std::pair<float, int>>, ComparePriority> queue;
};

#endif //ZMQ_COMM_SWARM_H
