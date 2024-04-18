import torch


def print_tensor(x):
    print("Python called")
    print("Python side has received a tensor of shape:" + str(x.shape) + " and dtype:" + str(
        x.dtype) + " and device:" + str(x.device))
    return x * -1
