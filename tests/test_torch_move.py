import torch
import time


def main():
    # 16k avg: 20us
    # 16MB avg: 3ms
    # concat 100 tensors avg: 200us

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
    print(f"Time taken to move a tensor ({len(x)}) to GPU: {(end - start) / num_rounds}")

    # Move a tensor to CPU
    x = x.to('cuda')
    start = time.time()
    for i in range(num_rounds):
        x.to('cpu')
    end = time.time()
    print(f"Time taken to move a tensor ({len(x)}) to CPU: {(end - start) / num_rounds}")

    # concat 1000 tensors
    tensor_list = [torch.randn(8192, dtype=torch.float16) for _ in range(100)]
    start = time.time()
    for i in range(100):
        concatenated_tensor = torch.cat(tensor_list, dim=0)
    end = time.time()
    print(f"Time taken to concatenate 100 tensors ({len(tensor_list[0])}): {(end - start) / num_rounds}")


if __name__ == '__main__':
    main()
