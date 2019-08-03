#!/bin/bash

clear

eval=${1-eval}
eval=$(realpath ${eval} | xargs -n1 basename)

grep -oP '\K.*' ${eval}/ckpt/*/eval_results.txt /dev/null | sed -e "s|.*ckpt/|${eval}  :  |" -e 's|/eval_results.txt:| : |' | column -t
