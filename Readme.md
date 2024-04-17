## Known Problems

1. PUB-SUB loses the first few requests (always).
2. ZMQ may discard requests when the system is near OOM.

## Notes

1. There can be only one context within each process.
2. Each socket can only be accessed by a single thread.
3. PUB-SUB's filter happens on the publisher side
4. Typically, we create one IO thread per GB/s.
5. Inproc sockets can be used for communication between threads (very fast).

## Solutions
1. -DCMAKE_CUDA_COMPILER=/usr/local/cuda/bin/nvcc (add this to pycharm cmake options)
