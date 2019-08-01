#!/bin/bash

CMD=${@:-/bin/bash}
NVD=${NVIDIA_VISIBLE_DEVICES:-"all"}

tmux new-session -A -s FinBERT \
docker run --rm -it \
    --name FinBERT \
    --net=host \
    --shm-size=1g \
    --ulimit memlock=-1 \
    --ulimit stack=67108864 \
    --gpus all \
    -e NVIDIA_VISIBLE_DEVICES=$NVD \
    -v $PWD:/w266-final \
    -w /w266-final/work \
    nvcr.io/nvidia/tensorflow:19.06-py3 $CMD
