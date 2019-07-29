#!/bin/bash

lspci | grep -i nvidia

  os=ubuntu1804
cuda=10.0.130-1

sudo apt-get update
sudo apt-get install -yq --autoremove \
     linux-headers-$(uname -r)
 
wget http://developer.download.nvidia.com/compute/cuda/repos/${os}/x86_64/cuda-repo-${os}_${cuda}_amd64.deb
sudo dpkg -i cuda-repo-${os}_${cuda}_amd64.deb
sudo apt-key adv --fetch-keys https://developer.download.nvidia.com/compute/cuda/repos/${os}/x86_64/7fa2af80.pub

sudo apt-get update
sudo apt-get install -yq --autoremove \
     cuda
