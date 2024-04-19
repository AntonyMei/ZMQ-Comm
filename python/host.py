# llm sys must be imported before torch
import time
import llm_host
import torch

import utils


def main():
    # warm up gpu and initialize llm_sys
    utils.warm_up()
    host_ip: str = utils.get_local_ip()
    assert host_ip.startswith("10"), "Local IP must be of form 10.xxx.xxx.xxx"
    llm_host.host_start_network_threads(utils.CONFIG_BROADCAST_ADDR, host_ip, "./config.txt")


    # TODO: below is not finished
    time.sleep(20)
    return


if __name__ == '__main__':
    main()
