# llm sys must be imported before torch
import time
import llm_worker
import torch

import utils


def main():
    # warm up gpu and initialize llm_sys
    utils.warm_up()
    worker_ip: str = utils.get_local_ip()
    assert worker_ip.startswith("10"), "Local IP must start with 10"
    llm_worker.worker_start_network_threads(utils.CONFIG_BROADCAST_ADDR, worker_ip)


    # TODO: below is not finished
    time.sleep(20)
    return

    # fetch new requests to compute on
    start_time = time.time()
    request_ids, is_prompt, start_layer_idx, end_layer_idx, num_tokens, max_tokens, tensors = llm_sys.worker_fetch_new_requests()
    print("Time to fetch new requests: ", time.time() - start_time)

    # dummy compute
    result_tensor = torch.cat(tensors, dim=0)
    request_ids.sort()
    start_idx = [8192 * 1000 * i for i in request_ids]
    lengths = [8192 * 1000 for _ in request_ids]
    start_time = time.time()
    llm_sys.worker_submit_requests(request_ids, start_idx, lengths, result_tensor)
    print("Time to submit results: ", time.time() - start_time)
    time.sleep(20)


if __name__ == '__main__':
    main()
