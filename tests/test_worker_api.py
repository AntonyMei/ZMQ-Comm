import time
# llm sys must be imported before torch
import llm_sys
import torch


def main():
    # create a tensor and move it to GPU (Warm up GPU)
    x = torch.tensor([1, 2, 3])
    for i in range(100):
        x.cuda()

    # test worker functions
    llm_sys.worker_start_network_threads()
    time.sleep(5)
    start_time = time.time()
    res = llm_sys.worker_fetch_new_requests()
    print("Time to fetch new requests: ", time.time() - start_time)
    print(res)


if __name__ == '__main__':
    main()
