 #!/bin/bash
 
lspci | grep -i nvidia

sudo apt-get update
sudo apt-get install -yq --autoremove \
     linux-headers-$(uname -r)
 
wget http://developer.download.nvidia.com/compute/cuda/repos/ubuntu1804/x86_64/cuda-repo-ubuntu1804_10.1.168-1_amd64.deb
sudo dpkg -i cuda-repo-ubuntu1804_10.1.168-1_amd64.deb
sudo apt-key adv --fetch-keys https://developer.download.nvidia.com/compute/cuda/repos/ubuntu1804/x86_64/7fa2af80.pub

sudo apt-get update
sudo apt-get install -yq --autoremove \
     cuda
