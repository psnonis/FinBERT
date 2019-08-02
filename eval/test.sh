#! /bin/bash

clear

[[ ! -e ../bert ]] && echo -e "\nPlease call from [eval] folder, exiting...\n" && exit 1

bert=$(realpath ../bert)
base=$(realpath ../base)
data=$(realpath  ./2019)
ckpt=$(realpath  ./ckpt)
work=$(realpath  .     )
  ts=$(realpath  ./ts  )

FIN=${1:-FinBERT-Error}
MSL=${2:-128}

# Pre-Training History
# 128 x 128 : OOM
# 128 x  96 : 15328 / 16130
# 512 x  96 : OOM
# 512 x  15 : Good

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
CFG=${base}/uncased_L-12_H-768_A-12/bert_config.json
DAT=${data}/????.tfrecord.??-${MSL}-${MPS}
OUT=${ckpt}/${FIN}
LOG=${ckpt}/${FIN}/events.out.testing.${DTS}.${MSL}.log

echo
echo Testing : ${FIN}
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

CMD+=" --do_train=False"
CMD+=" --do_eval=True"

CMD+=" --max_seq_length=${MSL}"
CMD+=" --max_predictions_per_seq=${MPS}"

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

echo ${FIN}  > ${LOG}
echo ${@}   >> ${LOG}
echo ${CMD} >> ${LOG}
echo        >> ${LOG}

mkdir -p ${OUT}
cd ${bert}

${CMD} |& ${ts} -s | tee ${LOG}

