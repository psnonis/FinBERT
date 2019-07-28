# Check NVIDIA devices
  lspci | grep -i nvidia

# Clean-up
# dpkg -l | grep cuda- | awk '{print $2}' | xargs -n1 sudo dpkg --purge

# Add NVIDIA package repositories
  wget https://developer.download.nvidia.com/compute/cuda/repos/ubuntu1804/x86_64/cuda-repo-ubuntu1804_10.0.130-1_amd64.deb
  sudo dpkg -i cuda-repo-ubuntu1804_10.0.130-1_amd64.deb
  sudo apt-key adv --fetch-keys https://developer.download.nvidia.com/compute/cuda/repos/ubuntu1804/x86_64/7fa2af80.pub
  sudo apt-get update
  wget http://developer.download.nvidia.com/compute/machine-learning/repos/ubuntu1804/x86_64/nvidia-machine-learning-repo-ubuntu1804_1.0.0-1_amd64.deb
  sudo apt install ./nvidia-machine-learning-repo-ubuntu1804_1.0.0-1_amd64.deb
  sudo apt-get update

# Install NVIDIA driver
  sudo apt-get install -yq --no-install-recommends nvidia-driver-418
# Reboot. Check that GPUs are visible using the command: nvidia-smi
  nvidia-smi

# Install development and runtime libraries (~4GB)
  sudo apt-get install -yq --no-install-recommends \
       cuda-10-0 \
       libcudnn7=7.6.0.64-1+cuda10.0  \
       libcudnn7-dev=7.6.0.64-1+cuda10.0

# Install TensorRT. Requires that libcudnn7 is installed above.
  sudo apt-get update
  sudo apt-get install -yq --no-install-recommends libnvinfer-dev=5.1.5-1+cuda10.0