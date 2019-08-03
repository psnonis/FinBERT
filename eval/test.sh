#! /bin/bash

clear

[[ ! -e ../bert ]] && echo -e "\nPlease call from [eval] folder, exiting...\n" && exit 1


bert=$(realpath ../bert)
base=$(realpath ../base)/uncased_L-12_H-768_A-12
data=$(realpath  ./eval)/data
ckpt=$(realpath  ./eval)/ckpt
work=$(realpath  .     )
  ts=$(realpath ../tool)/ts

FIN=${1:-FinBERT-Error}
MSL=${2:-128}
NTS=${3:-0}

MSL=128 # maximum sequence length
MPS=20  # maximum predictions/masks per sequence (15% of MSL)

DTS=`date +'%m%d%H%M%S'`
CFG=${base}/bert_config.json
DAT=${data}/????.tfrecord.??-${MSL}-${MPS}
OUT=${ckpt}/${FIN}
LOG=${ckpt}/${FIN}/events.out.testing.${DTS}.${MSL}.log

echo
echo Testing : ${FIN}
echo
echo CFG : ${CFG}
echo
echo DAT : ${DAT}
echo
echo OUT : ${OUT}
echo

CMD="python3 -W ignore ${bert}/run_pretraining.py"

CMD+=" --bert_config_file=${CFG}"
CMD+=" --input_file=${DAT}"
CMD+=" --output_dir=${OUT}"

CMD+=" --do_train=False"
CMD+=" --do_eval=True"

CMD+=" --max_seq_length=${MSL}"
CMD+=" --max_predictions_per_seq=${MPS}"
CMD+=" --num_training_steps=${NTS}"

echo LOG : ${LOG}
echo
echo CMD : ${CMD}
echo

cd ${bert}

${CMD} |& ${ts} -s | tee ${LOG}

