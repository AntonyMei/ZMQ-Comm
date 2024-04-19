import sys
import torch

# Add the path to the directory containing the .so file
sys.path.append('/workspace/tests/minimal_test')
print(sys.path)

# Import the module
# Note: mv test_python_call.cpython-310-x86_64-linux-gnu.so example.so
import test
print("Successfully imported the module")

# Use the function
print(test.test_tensor())