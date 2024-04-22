# llm sys must be imported before torch
import time
import llm_host
import torch

import utils


def main():
    # warm up gpu and initialize network threads
    utils.warm_up()
    host_ip: str = utils.get_local_ip()
    assert host_ip.startswith("10"), "Local IP must be of form 10.xxx.xxx.xxx"
    llm_host.start_network_threads(utils.CONFIG_BROADCAST_ADDR, host_ip, "./config.txt", "maxflow")
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
            True,  # set_routing
            [1, 3, 0],  # server_ids
            [0, 2, -1],  # start_layer_ids
            [2, 4, -1],  # end_layer_ids
        )
        end = time.time()
        print(f"prompt delta={end - start}")
        start = time.time()
        for i in range(3):
            llm_host.launch_request(
                "decode",  # request_type
                2 + i,  # request_id
                750,  # num_tokens
                2000,  # max_num_tokens
                [-1],  # token_ids
                True,  # set_routing
                [1, 3, 0],  # server_ids
                [0, 2, -1],  # start_layer_ids
                [2, 4, -1],  # end_layer_ids
            )
        end = time.time()
        print(f"decode speed={3 / (end - start)}")
        # ------------------------------------------------------------------------------------------- #
        # Step 1: Gather finished requests
        time.sleep(10)
        start = time.time()
        finished_request_ids, generated_token_ids, routes, num_layers = llm_host.gather_finished_requests()
        end = time.time()
        print(f"gather delta={end - start}")
        print(f"finished_request_ids={finished_request_ids}")
        print(f"generated_token_ids={generated_token_ids}")
        print(f"routes={routes}")
        print(f"num_layers={num_layers}")
        # ------------------------------------------------------------------------------------------- #
        # Step 1: Launch a few requests
        start = time.time()
        llm_host.launch_request(
            "decode",  # request_type
            1,  # request_id
            750,  # num_tokens
            2000,  # max_num_tokens
            [-1],  # token_ids
            True,  # set_routing
            [1, 3, 0],  # server_ids
            [0, 2, -1],  # start_layer_ids
            [2, 4, -1],  # end_layer_ids
        )
        end = time.time()
        print(f"prompt delta={end - start}")
        # ------------------------------------------------------------------------------------------- #
        # Step 1: Gather finished requests
        time.sleep(10)
        start = time.time()
        finished_request_ids, generated_token_ids, routes, num_layers = llm_host.gather_finished_requests()
        end = time.time()
        print(f"gather delta={end - start}")
        print(f"finished_request_ids={finished_request_ids}")
        print(f"generated_token_ids={generated_token_ids}")
        print(f"routes={routes}")
        print(f"num_layers={num_layers}")
        # ------------------------------------------------------------------------------------------- #
        time.sleep(3000)  # for testing


if __name__ == '__main__':
    main()
