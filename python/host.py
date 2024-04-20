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

    # TODO: finish the python side main loop
    while True:
        # data = [1] * 750
        # start = time.time()
        # llm_host.launch_request(
        #     "prompt",
        #     1,
        #     750,
        #     2000,
        #     data,
        #     True,
        #     [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15],
        #     [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15],
        #     [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15]
        # )
        # end = time.time()
        # print(f"prompt delta={end - start}")
        start = time.time()
        for _ in range(1000):
            llm_host.launch_request(
                "decode",
                1,
                750,
                2000,
                [1],
                True,
                [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15],
                [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15],
                [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15]
            )
        end = time.time()
        print(f"decode speed={1000 / (end - start)}")
        time.sleep(5)


if __name__ == '__main__':
    main()
