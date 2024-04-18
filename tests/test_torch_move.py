import torch
import time


def main():
    # 16k avg: 20us
    # 16MB avg: 3ms

    # create a tensor with 8k elements of fp16
    x = torch.randn(8192, dtype=torch.float16)          # 16k
    # x = torch.randn(8192 * 1000, dtype=torch.float16)   # 16M
    num_rounds = 100

    # warm up
    for i in range(100):
        x.to('cuda')

    # Move a tensor to GPU
    start = time.time()
    for i in range(num_rounds):
        x.to('cuda')
    end = time.time()
    print((end - start) / num_rounds)

    # Move a tensor to CPU
    x = x.to('cuda')
    start = time.time()
    for i in range(num_rounds):
        x.to('cpu')
    end = time.time()
    print((end - start) / num_rounds)


if __name__ == '__main__':
    main()
