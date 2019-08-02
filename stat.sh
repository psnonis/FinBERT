#!/bin/bash

[ ! -e /usr/local/bin/gpustat ] && pip3 -q install gpustat
gpustat -i 5 -c -F -P
