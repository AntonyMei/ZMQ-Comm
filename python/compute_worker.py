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
    llm_worker.start_network_threads(utils.CONFIG_BROADCAST_ADDR, worker_ip, "maxflow")
    start_idx, end_idx, is_last_layer = llm_worker.get_model_start_end_idx()
    print(f"[Python] Cluster initialization finished!")
    print(f"[Python] Model layers: [{start_idx}, {end_idx}).")
    print(f"[Python] Does this node output the last layer: {is_last_layer}.")

    while True:
        # ------------------------------------------------------------------------------------------- #
        # Step 1: fetch new requests to compute on
        start_time = time.time()
        request_ids, is_prompt, start_layer_idx, end_layer_idx, num_tokens, max_tokens, offsets, \
            lengths, is_token_tensor, token_tensor, activation_tensor = llm_worker.fetch_new_requests()
        if len(request_ids) == 0:
            continue
        print("Time to fetch new requests: ", time.time() - start_time)
        print("Request ids: ", request_ids)
        print("Is prompt: ", is_prompt)
        print("Start layer idx: ", start_layer_idx)
        print("End layer idx: ", end_layer_idx)
        print("Num tokens: ", num_tokens)
        print("Max tokens: ", max_tokens)
        print("Offsets: ", offsets)
        print("Lengths: ", lengths)
        print("Is token tensor: ", is_token_tensor)
        print("Token Tensor: ", token_tensor)
        print("Activation Tensor: ", activation_tensor)
        # ------------------------------------------------------------------------------------------- #
        # Step 2: dummy compute (suppose activation size is 16 for each token)
        activation_size = 16
        if not is_last_layer:
            cur_start_idx = 0
            start_idx_list, length_list = [], []
            for is_prompt, num_token in zip(is_prompt, num_tokens):
                start_idx_list.append(cur_start_idx)
                if is_prompt:
                    length_list.append(num_token * activation_size)
                    cur_start_idx += num_token * activation_size
                else:
                    length_list.append(1 * activation_size)
                    cur_start_idx += 1 * activation_size
            result_tensor = torch.arange(1, cur_start_idx + 1, dtype=torch.float16)  # a flattened tensor
        else:
            output_token_ids = [-1 if is_p else 1 for is_p in is_prompt]
            start_idx_list = [i for i in range(len(request_ids))]
            length_list = [1 for _ in range(len(request_ids))]
            result_tensor = torch.tensor(output_token_ids, dtype=torch.int32)
        print("Result tensor size: ", result_tensor.size())
        print("Result tensor dtype: ", result_tensor.dtype)
        print("Start idx list: ", start_idx_list)
        print("Length list: ", length_list)
        # ------------------------------------------------------------------------------------------- #
        # Step 3: submit results
        start_time = time.time()
        llm_worker.submit_requests(request_ids, start_idx_list, length_list, result_tensor)
        print("Time to submit results: ", time.time() - start_time)
        print()
        # ------------------------------------------------------------------------------------------- #


if __name__ == '__main__':
    main()
