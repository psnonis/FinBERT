#! /bin/bash

clear

[[ ! -e ../bert ]] && echo -e "\nPlease call from [work] folder, exiting...\n" && exit 1

bert=$(realpath ../bert)
base=$(realpath ../base)/uncased_L-12_H-768_A-12
data=$(realpath  ./2019)
ckpt=$(realpath  ./step)
work=$(realpath  .     )
  ts=$(realpath  ./ts  )

FIN=${1:-GooBERT}
MSL=${2:-128}
NTS=${3:-1}
NWS=${4:-0}
TBS=${5:-1}

if [[ ${MSL} == 128 ]]; then

  MSL=128 # maximum sequence length
  MPS=20  # maximum predictions/masks per sequence (15% of MSL)
# TBS=96  # training batch size

else

  MSL=512 # maximum sequence length
  MPS=80  # maximum predictions/masks per sequence (15% of MSL)
# TBS=96  # training batch size

fi

DTS=`date +'%m%d%H%M%S'`
CFG=${base}/bert_config.json
DAT=${data}/????.tfrecord.??-${MSL}-${MPS}

OUT=${ckpt}/${FIN}
LOG=${ckpt}/${FIN}/events.out.training.${DTS}.${MSL}x${TBS}.${NTS}.log
INI=${base}/bert_model.ckpt

echo
echo Pre-Training : ${FIN}
echo
echo CFG : ${CFG}
echo
echo INI : ${INI}
echo
echo DAT : ${DAT}
echo
echo OUT : ${OUT}
echo

CMD="python3 -W ignore ${bert}/run_pretraining.py"

CMD+=" --bert_config_file=${CFG}"
CMD+=" --input_file=${DAT}"
CMD+=" --output_dir=${OUT}"

CMD+=" --init_checkpoint=${INI}"

CMD+=" --do_train=True"
CMD+=" --do_eval=True"

CMD+=" --train_batch_size=${TBS}"
CMD+=" --max_seq_length=${MSL}"
CMD+=" --max_predictions_per_seq=${MPS}"

CMD+=" --num_train_steps=${NTS}"
CMD+=" --num_warmup_steps=${NWS}"

CMD+=" --save_checkpoints_steps=1"

CMD+=" --learning_rate=0"
CMD+=" --report_loss"

echo LOG : ${LOG}
echo
echo CMD : ${CMD}
echo
echo FIN : ${FIN} : $(ls ${OUT})
echo
echo -n "ASK : Looks Good ? (Press Enter to Continue or Ctrl+C to Exit) "
read
echo

mkdir -p ${OUT}

echo ${FIN}  > ${LOG}
echo ${@}   >> ${LOG}
echo ${CMD} >> ${LOG}
echo        >> ${LOG}

cd ${bert}

${CMD} |& ${ts} -s | tee ${LOG}

