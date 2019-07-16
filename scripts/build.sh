#!/bin/bash

      root=$(readlink -f ${0} | xargs dirname | xargs dirname)
containers=$(realpath --relative-to=${PWD} ${root}/containers)
   context=${1-pythia}
    expose=${2-5000}
repository=psnonis/w251-final

PAD=$'\x1B[K'
SKY=$'\x1B[0;37;44m'
GRN=$'\x1B[0;30;42m'
TXT=$'\x1B[38;5;190m'
RST=$'\x1B[0m'
EOL=$'\n'

 build="docker build -t ${repository}:${context} -f ${containers}/${context}/Dockerfile ${containers}/${context}"
header="Container : ${repository}:${context} ${PAD}${EOL}   Command : ${build}"
logger="tee ${containers}/${context}/build.log"
 color="ack --passthru ^Step.*"

echo -e "${EOL}${SKY}${TXT}${PAD}${EOL} ${header} ${PAD}${EOL}${PAD}${RST}${EOL}"

${build} | ${logger} | ${color}

echo -e "${EOL}${GRN}${TXT}${PAD}${RST}${EOL}"
