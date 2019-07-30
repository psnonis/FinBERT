#! /bin/bash

# bert.sh 512 0 - for 512 MSL and 

clear

[[ ! -e ../bert-gpu ]] && echo -e "\nPlease call from a [work-xxx] folder, exiting...\n" && exit 1

bert=$(realpath ../bert-gpu)
base=$(realpath ../base)
data=$(realpath  ./data)
ckpt=$(realpath  ./ckpt)
work=$(realpath  .     )

MSL=${1:-128}
BWI=${2:-0}

# Pre-Training History
# 128 x 128 : OOM
# 128 x  96 : 15328 / 16130

echo "${MSL}" == 128

if [[ ${MSL} == 128 ]]; then

  MSL=128 # maximum sequence length
  MPS=20  # maximum predictions/masks per sequence (15% of MSL)
  TBS=96  # training batch size

else

  MSL=512 # maximum sequence length
  MPS=80  # maximum predictions/masks per sequence (15% of MSL)
  TBS=32  # training batch size

fi

GPU=2
DTS=`date +'%y-%m-%d_%H-%M-%S'`
CFG=${base}/uncased_L-12_H-768_A-12/bert_config.json
DAT=${data}/????.tfrecord.??-${MSL}-${MPS}

if [[ ${work} == *"pre"* ]]; then
  if [[ ${BWI} == 1 ]]; then
    FIN=FinBERT-Error
  else
    FIN=FinBERT-Pre2K
  fi
fi

if [[ ${work} == *"fin"* ]]; then
  if [[ ${BWI} == 1 ]]; then
    FIN=FinBERT-Combo
  else
    FIN=FinBERT-Prime
  fi
fi

if [[ ${BWI} == 1 ]]; then

# BERT Weights Initialization
  OUT=${ckpt}/${MSL}-${MPS}_${TBS}_bwi
  LOG=${ckpt}/${MSL}-${MPS}_${TBS}_bwi/pretraining.${DTS}.log
  INI=${base}/uncased_L-12_H-768_A-12

else

# From Scratch Initialization
  OUT=${ckpt}/${MSL}-${MPS}_${TBS}_fsi
  LOG=${ckpt}/${MSL}-${MPS}_${TBS}_fsi/pretraining.${DTS}.log
  INI=${OUT}

fi


echo
echo ${FIN}_${MSL}MSL

echo
echo CFG : ${CFG}
echo
echo INI : ${INI}
echo
echo DAT : ${DAT}
echo
echo OUT : ${OUT}
echo

CMD="python3 /workspace/bert/run_pretraining.py"

CMD+=" --bert_config_file=${CFG}"
CMD+=" --input_file=${DAT}"
CMD+=" --output_dir=${OUT}"
CMD+=" --init_checkpoint=${INI}"

CMD+=" --do_train=True"
CMD+=" --do_eval=True"

CMD+=" --train_batch_size=${bat_sze}"
CMD+=" --max_seq_length=${max_seq}"
CMD+=" --max_predictions_per_seq=${max_pre}"

CMD+=" --num_train_steps=250000"
CMD+=" --num_warmup_steps=10000"

CMD+=" --learning_rate=1e-4"
CMD+=" --use_fp16"
CMD+=" --use_xla"
CMD+=" --report_loss"
CMD+=" --horovod"

if [ $GPU -gt 1 ] ; then
  CMD="mpiexec --allow-run-as-root -np ${GPU} --bind-to socket ${CMD}"
fi

echo CMD : ${CMD}
echo
echo CFG : ${FIN}_${MSL}MSL
echo
echo -n "ASK : Looks Good ? (Press Enter to Continue or Ctrl+C to Exit) "
read
echo

mkdir -p ${OUT}
cd ${bert}

( ${CMD} ) |& tee $LOG
