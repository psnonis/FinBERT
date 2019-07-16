#!/bin/bash

      root=$(readlink -f ${0} | xargs dirname | xargs dirname)
containers=$(realpath --relative-to=${PWD} ${root}/containers)
   context=${1-pythia}
    expose=${2}
repository=psnonis/w251-final

PAD=$'\x1B[K'
SKY=$'\x1B[0;37;44m'
GRN=$'\x1B[0;30;42m'
TXT=$'\x1B[38;5;190m'
RST=$'\x1B[0m'
EOL=$'\n'

  clean="docker rm -f ${context}"
command="docker run -t -p ${expose}:${expose} --rm --name ${context} ${repository}:${context}"
 header="Container : ${repository}:${context} ${PAD}${EOL}   Command : ${command}"

echo -e "${EOL}${SKY}${TXT}${PAD}${EOL} ${header} ${PAD}${EOL}${PAD}${RST}${EOL}"

${clean} 2> /dev/null
${command}

echo -e "${EOL}${GRN}${TXT}${PAD}${RST}${EOL}"