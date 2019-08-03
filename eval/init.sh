#!/bin/bash

clear

#rm -rf eval/ckpt/*

data=$(realpath eval | sed 's:eval:data:')
echo $data

for m in $(ls -d /gold/*/ | xargs -n1 basename)
do
  mkdir -p eval/ckpt/$m && ln -sf /gold/$m/*ckpt* eval/ckpt/$m && ln -sf /gold/$m/c* eval/ckpt/$m
done

ls -l --color  eval/ckpt
ln -sf ${data} eval/data
