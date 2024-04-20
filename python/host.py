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
    llm_host.host_start_network_threads(utils.CONFIG_BROADCAST_ADDR, host_ip, "./config.txt")

    # TODO: finish the python side main loop
    while True:
        time.sleep(5)


if __name__ == '__main__':
    main()
