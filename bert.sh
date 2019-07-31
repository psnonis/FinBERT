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
NTS=${2:-250000}
BWI=${3:-0}

# Pre-Training History
# 128 x 128 : OOM
# 128 x  96 : 15328 / 16130

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
    FIN=FinBERT-Error_${MSL}MSL
  else
    FIN=FinBERT-Pre2K_${MSL}MSL
  fi
fi

if [[ ${work} == *"fin"* ]]; then
  if [[ ${BWI} == 1 ]]; then
    FIN=FinBERT-Combo_${MSL}MSL
  else
    FIN=FinBERT-Prime_${MSL}MSL
  fi
fi

if [[ ${BWI} == 1 ]]; then

# BERT Weights Initialization
  OUT=${ckpt}/${FIN}
  LOG=${ckpt}/${FIN}/pretraining.${DTS}.log
  INI=${base}/uncased_L-12_H-768_A-12/bert_model.ckpt

else

# From Scratch Initialization
  OUT=${ckpt}/${FIN}
  LOG=${ckpt}/${FIN}/pretraining.${DTS}.log
# INI=${OUT}/model.ckpt

fi


echo
echo ${FIN} Pre-Training

echo
echo CFG : ${CFG}
echo
echo INI : ${INI}
echo
echo DAT : ${DAT}
echo
echo OUT : ${OUT}
echo

CMD="python3 ${bert}/run_pretraining.py"

CMD+=" --bert_config_file=${CFG}"
CMD+=" --input_file=${DAT}"
CMD+=" --output_dir=${OUT}"


#CMD+=" --init_checkpoint=${INI}"

CMD+=" --do_train=True"
CMD+=" --do_eval=True"

CMD+=" --train_batch_size=${TBS}"
CMD+=" --max_seq_length=${MSL}"
CMD+=" --max_predictions_per_seq=${MPS}"

CMD+=" --num_train_steps=${NTS}"
CMD+=" --num_warmup_steps=10000"

CMD+=" --save_checkpoints_steps=10000"

CMD+=" --learning_rate=1e-4"
CMD+=" --use_fp16"
CMD+=" --use_xla"
CMD+=" --report_loss"
CMD+=" --horovod"

if [ $GPU -gt 1 ] ; then
  CMD="mpiexec --allow-run-as-root -np ${GPU} --bind-to socket ${CMD}"
fi

echo LOG : ${LOG}
echo
echo CMD : ${CMD}
echo
echo FIN : ${FIN}
echo
echo -n "ASK : Looks Good ? (Press Enter to Continue or Ctrl+C to Exit) "
read
echo

mkdir -p ${OUT}
cd ${bert}

( ${CMD} ) |& sed -u "s/^/$(date +%X) /" | tee $LOG
