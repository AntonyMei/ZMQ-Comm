# llm sys must be imported before torch
import time
import llm_host
import torch

import utils

# difference with host:
# 1. routing method set as swarm
# 2. no routing info for requests


def main():
    # warm up gpu and initialize network threads
    utils.warm_up()
    host_ip: str = utils.get_local_ip()
    assert host_ip.startswith("10"), "Local IP must be of form 10.xxx.xxx.xxx"
    llm_host.start_network_threads(utils.CONFIG_BROADCAST_ADDR, host_ip, "./config.txt", "swarm")
    print("[Python] Cluster initialization finished!")

    while True:
        # ------------------------------------------------------------------------------------------- #
        # Step 1: Launch a few requests
        start = time.time()
        llm_host.launch_request(
            "prompt",  # request_type
            1,  # request_id
            750,  # num_tokens
            2000,  # max_num_tokens
            [i for i in range(750)],  # token_ids
            False,  # set_routing
            [],  # server_ids
            [],  # start_layer_ids
            [],  # end_layer_ids
        )
        llm_host.launch_request(
            "prompt",  # request_type
            2,  # request_id
            500,  # num_tokens
            2000,  # max_num_tokens
            [i for i in range(500)],  # token_ids
            False,  # set_routing
            [],  # server_ids
            [],  # start_layer_ids
            [],  # end_layer_ids
        )
        end = time.time()
        print(f"prompt delta={end - start}")
        # ------------------------------------------------------------------------------------------- #
        # Step 1: Gather finished requests
        time.sleep(10)
        start = time.time()
        finished_request_ids, generated_token_ids = llm_host.gather_finished_requests()
        end = time.time()
        print(f"gather delta={end - start}")
        print(f"finished_request_ids={finished_request_ids}")
        print(f"generated_token_ids={generated_token_ids}")
        # ------------------------------------------------------------------------------------------- #
        # Step 1: Launch a few requests
        start = time.time()
        llm_host.launch_request(
            "decode",  # request_type
            1,  # request_id
            750,  # num_tokens
            2000,  # max_num_tokens
            [-1],  # token_ids
            False,  # set_routing
            [],  # server_ids
            [],  # start_layer_ids
            [],  # end_layer_ids
        )
        llm_host.launch_request(
            "decode",  # request_type
            2,  # request_id
            500,  # num_tokens
            2000,  # max_num_tokens
            [-1],  # token_ids
            False,  # set_routing
            [],  # server_ids
            [],  # start_layer_ids
            [],  # end_layer_ids
        )
        end = time.time()
        print(f"prompt delta={end - start}")
        # ------------------------------------------------------------------------------------------- #
        # Step 1: Gather finished requests
        time.sleep(10)
        start = time.time()
        finished_request_ids, generated_token_ids = llm_host.gather_finished_requests()
        end = time.time()
        print(f"gather delta={end - start}")
        print(f"finished_request_ids={finished_request_ids}")
        print(f"generated_token_ids={generated_token_ids}")
        # ------------------------------------------------------------------------------------------- #
        time.sleep(3000)  # for testing


if __name__ == '__main__':
    main()
