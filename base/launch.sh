#!/bin/bash

CMD=${@:-/bin/bash}
NVD=${NVIDIA_VISIBLE_DEVICES:-"all"}

docker run --rm -it \
    --net=host \
    --shm-size=1g \
    --ulimit memlock=-1 \
    --ulimit stack=67108864 \
    --gpus all \
    -e NVIDIA_VISIBLE_DEVICES=$NVD \
    -v $PWD:/w266-final \
    nvcr.io/nvidia/tensorflow:19.06-py3 $CMD
