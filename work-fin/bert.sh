#! /bin/bash

bert=$(realpath ../bert-gpu)
base=$(realpath ../base)
data=$(realpath  ./data)
ckpt=$(realpath  ./ckpt)

max_seq=128
max_pre=20
bat_sze=96

mkdir -p ${ckpt}/${max_seq}-${max_pre}_${bat_sze}
  rm -rf ${ckpt}/${max_seq}-${max_pre}_${bat_sze}/*

echo ${bert} ${max_seq} ${max_pre} ${bat_sze}
echo ${data}/*

  cd ${bert}

# 128 + 128 : OOM
# 128 +  96 : 15328 / 16130

mpiexec --allow-run-as-root --bind-to socket -np 2 python3 run_pretraining.py \
  --bert_config_file=${base}/uncased_L-12_H-768_A-12/bert_config.json \
  --input_file=${data}/????.tfrecord.??-${max_seq}-${max_pre} \
  --output_dir=${ckpt}/${max_seq}-${max_pre}_${bat_sze} \
  --do_train=True \
  --do_eval=True \
  --train_batch_size=${bat_sze} \
  --max_seq_length=${max_seq} \
  --max_predictions_per_seq=${max_pre} \
  --num_train_steps=250000 \
  --num_warmup_steps=10000 \
  --learning_rate=1e-4 \
  --use_fp16 \
  --use_xla \
  --report_loss \
  --horovod
