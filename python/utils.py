import torch
import socket

CONFIG_BROADCAST_ADDR = "tcp://10.128.0.53:5000"


def warm_up():
    # create a tensor and move it to GPU (Warm up GPU)
    x = torch.tensor([1, 2, 3])
    for i in range(100):
        x.cuda()


def get_local_ip():
    # Attempt to connect to an internet host in order to determine the local interface
    try:
        # Create a dummy socket to connect to an Internet IP or DNS
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
            # Use Google's public DNS server to find out our IP
            s.connect(("8.8.8.8", 80))
            ip = s.getsockname()[0]
        return ip
    except Exception as e:
        return f"Error obtaining local IP: {str(e)}"
