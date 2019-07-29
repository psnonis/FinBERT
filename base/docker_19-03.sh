#!/bin/bash

sudo apt-get update
sudo apt-get install -yq \
     apt-transport-https \
     ca-certificates \
     curl \
     software-properties-common

curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo apt-key add -
sudo apt-key fingerprint 0EBFCD88
sudo add-apt-repository \
     "deb [arch=amd64] https://download.docker.com/linux/ubuntu \
     $(lsb_release -cs) \
     stable"

sudo apt-get install nvidia-container-runtime
type nvidia-container-runtime-hook

sudo apt-get update
sudo apt-get install -yq --autoremove \
     docker-ce docker-ce-cli containerd.io

docker version
docker run -it --rm --gpus all ubuntu nvidia-smi

docker login -u '$oauthtoken' --password 'dWcycWEwMzZ1Nm92dmppOHR0dHUzMW9sbzM6Mzk3MWQyZjgtMmRhZi00YTA2LTllMmMtMjExOTljZTYzMGM4' nvcr.io
docker pull nvcr.io/nvidia/tensorflow:19.06-py3
