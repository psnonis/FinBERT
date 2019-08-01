#! /bin/bash

# bert.sh FinBERT-Prime 128 250000 10000 : for 128 MSL and 250K training steps and 10K warmup steps

clear

[[ ! -e ../bert-gpu ]] && echo -e "\nPlease call from [work] folder, exiting...\n" && exit 1

bert=$(realpath ../bert-gpu)
base=$(realpath ../base)
data=$(realpath  ./2019)
ckpt=$(realpath  ./ckpt)
work=$(realpath  .     )
  ts=$(realpath  ./ts  )

FIN=${1:-FinBERT-Error}
MSL=${2:-128}
NTS=${3:-250000}
NWS=${4:-10000}
TBS=${5:-96}
BWI=${6:-0}

# Pre-Training History
# 128 x 128 : OOM
# 128 x  96 : 15328 / 16130
# 512 x  96 : OOM

if [[ ${MSL} == 128 ]]; then

  MSL=128 # maximum sequence length
  MPS=20  # maximum predictions/masks per sequence (15% of MSL)
# TBS=96  # training batch size

else

  MSL=512 # maximum sequence length
  MPS=80  # maximum predictions/masks per sequence (15% of MSL)
# TBS=96  # training batch size

fi

GPU=2
DTS=`date +'%m%d%H%M%S'`
CFG=${base}/uncased_L-12_H-768_A-12/bert_config.json
DAT=${data}/????.tfrecord.??-${MSL}-${MPS}

FIN=${FIN}_${MSL}MSL

OUT=${ckpt}/${FIN}
LOG=${ckpt}/${FIN}/events.out.training.${DTS}.${MSL}x${TBS}.${NTS}.log

if [[ ${BWI} == 1 ]]; then
# BERT Weights Initialization
  INI=${base}/uncased_L-12_H-768_A-12/bert_model.ckpt
# else
# From Scratch Initialization
# INI=${OUT}/model.ckpt
fi

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

CMD="python3 ${bert}/run_pretraining.py"

CMD+=" --bert_config_file=${CFG}"
CMD+=" --input_file=${DAT}"
CMD+=" --output_dir=${OUT}"

[[ -n ${INI} ]] && CMD+=" --init_checkpoint=${INI}"

CMD+=" --do_train=True"
CMD+=" --do_eval=True"

CMD+=" --train_batch_size=${TBS}"
CMD+=" --max_seq_length=${MSL}"
CMD+=" --max_predictions_per_seq=${MPS}"

CMD+=" --num_train_steps=${NTS}"
CMD+=" --num_warmup_steps=${NWS}"

CMD+=" --save_checkpoints_steps=5000"

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
echo FIN : ${FIN} : $(ls ${OUT})
echo
echo -n "ASK : Looks Good ? (Press Enter to Continue or Ctrl+C to Exit) "
read
echo

echo ${FIN}  > ${LOG}
echo ${@}   >> ${LOG}
echo ${CMD} >> ${LOG}
echo        >> ${LOG}

mkdir -p ${OUT}
cd ${bert}

${CMD} |& ${ts} | tee ${LOG}

