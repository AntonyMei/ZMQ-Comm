import time
# llm sys must be imported before torch
import llm_sys
import torch


def main():
    # create a tensor and move it to GPU (Warm up GPU)
    x = torch.tensor([1, 2, 3])
    for i in range(100):
        x.cuda()

    # start the worker threads
    llm_sys.worker_start_network_threads()
    time.sleep(1)

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
