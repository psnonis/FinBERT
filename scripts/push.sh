#!/bin/bash

      root=$(readlink -f ${0} | xargs dirname | xargs dirname)
containers=$(realpath --relative-to=${PWD} ${root}/containers)
   context=${1-pythia}
repository=psnonis/w251-final

echo Pushing ${repository}:${context}
echo docker push ${repository}:${context}
     docker push ${repository}:${context}