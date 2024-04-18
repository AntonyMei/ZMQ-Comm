import sys

# Add the path to the directory containing the .so file
sys.path.append('/home/meiyixuan2000/llm_system/build')
print(sys.path)

# Import the module
# Note: mv test_python_call.cpython-310-x86_64-linux-gnu.so example.so
import example
print("Successfully imported the module")

# Use the function
example.run()  # This will start the two threads in the background
