#!/usr/bin/env bash

mkdir -p data
git clone https://github.com/google-research/bert
wget https://s3.amazonaws.com/models.huggingface.co/bert/bert-base-uncased-vocab.txt data
