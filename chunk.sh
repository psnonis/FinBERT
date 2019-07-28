#!/usr/bin/env bash

# expecting data/{year}.kevin
  year=${1-1999}

# convert {year}.kevin to {year}.chunk
  python3 chunk.py ${year}

# convert {year}.chunk to {year}.tfrecord
  python3 bert/create_pretraining_data.py \
  --input_file=data/${year}.chunk \
  --output_file=data/${year}.tfrecord \
  --vocab_file=data/bert-base-uncased-vocab.txt \
  --do_lower_case=True \
  --do_whole_word_mask=True \
  --max_predictions_per_seq=20 \
  --max_seq_length=540 \
  --masked_lm_prob=0.15 \
  --random_seed=42 \
  --dupe_factor=5