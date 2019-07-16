FROM pytorch/pytorch:latest

RUN pip install pytorch-pretrained-bert

RUN git clone https://github.com/NVIDIA/apex.git
WORKDIR /workspace/apex
RUN pip install -v --no-cache-dir --global-option="--cpp_ext" --global-option="--cuda_ext" ./

WORKDIR /workspace