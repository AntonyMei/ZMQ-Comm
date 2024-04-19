# llm sys must be imported before torch
import time
import llm_host
import torch

import utils


def main():
    # warm up gpu and initialize llm_sys
    utils.warm_up()
    ip: str = utils.get_local_ip()
    assert ip.startswith("10"), "Local IP must start with 10"
    llm_host.host_start_network_threads(utils.HOST_ADDR, ip)


    # TODO: below is not finished
    time.sleep(20)
    return


if __name__ == '__main__':
    main()
