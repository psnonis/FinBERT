#!/bin/bash

clear

for m in $(ls -dr eval/ckpt/* | xargs -n 1 basename)
do
if [[ ! -d eval/ckpt/${m}/eval ]];
then
  test.sh ${m} 128 0
fi
done 
