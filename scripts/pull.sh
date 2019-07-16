#!/bin/bash

      root=$(readlink -f ${0} | xargs dirname | xargs dirname)
containers=$(realpath --relative-to=${PWD} ${root}/containers)
   context=${1-pythia}
repository=psnonis/w251-final

echo Pulling ${repository}:${context}
echo docker pull ${repository}:${context}
     docker pull ${repository}:${context}