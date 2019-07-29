#!/bin/bash
# Copyright (c) 2018-2019, NVIDIA CORPORATION. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#  * Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#  * Neither the name of NVIDIA CORPORATION nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

CLIENT_LOG_BASE="./client"
INFER_TEST=infer_test.py

DATADIR=`pwd`/models

SERVER=/opt/tensorrtserver/bin/trtserver
# Allow more time to exit. Ensemble brings in too many models
SERVER_ARGS="--model-store=$DATADIR --exit-timeout-secs=120"
SERVER_LOG_BASE="./inference_server"
source ../common/util.sh

rm -f $SERVER_LOG_BASE* $CLIENT_LOG_BASE*

RET=0

# Verify the flag is set only on CPU-only device
if [ "$TENSORRT_SERVER_CPU_ONLY" == "1" ]; then
    gpu_count=`nvidia-smi -L | grep GPU | wc -l`
    if [ "$gpu_count" -ne 0 ]; then
    echo -e "\n***\n*** Running on a device with GPU\n***"
    echo -e "\n***\n*** Test Failed To Run\n***"
    exit 1
    fi
fi

for TARGET in cpu gpu; do
    if [ "$TENSORRT_SERVER_CPU_ONLY" == "1" ]; then
        if [ "$TARGET" == "gpu" ]; then
            echo -e "Skip GPU testing on CPU-only device"
            continue
        fi
        # set strict readiness=false on CPU-only device to allow
        # unsuccessful load of TensorRT plans, which require GPU.
        SERVER_ARGS="--model-store=$DATADIR --exit-timeout-secs=120 --strict-readiness=false"
    fi

    SERVER_LOG=$SERVER_LOG_BASE.${TARGET}.log
    CLIENT_LOG=$CLIENT_LOG_BASE.${TARGET}.log

    rm -fr models && \
        cp -r /data/inferenceserver/qa_model_repository models && \
        cp -r /data/inferenceserver/qa_ensemble_model_repository/qa_model_repository/* models/. && \
        cp -r ../custom_models/custom_float32_* models/. && \
        cp -r ../custom_models/custom_int32_* models/. && \
        cp -r ../custom_models/custom_nobatch_* models/.

    create_nop_modelfile `pwd`/libidentity.so `pwd`/models

    for EM in `ls ../ensemble_models`; do
        mkdir -p ../ensemble_models/$EM/1
    done
    cp -r ../ensemble_models/* models/.

    KIND="KIND_GPU" && [[ "$TARGET" == "cpu" ]] && KIND="KIND_CPU"
    # Onnx models are handled separately, see below
    for FW in graphdef savedmodel netdef onnx custom; do
        for MC in `ls models/${FW}*/config.pbtxt`; do
            echo "instance_group [ { kind: ${KIND} }]" >> $MC
        done
    done

    run_server
    if [ "$SERVER_PID" == "0" ]; then
        echo -e "\n***\n*** Failed to start $SERVER\n***"
        cat $SERVER_LOG
        exit 1
    fi

    set +e

    # python unittest seems to swallow ImportError and still return 0
    # exit code. So need to explicitly check CLIENT_LOG to make sure
    # we see some running tests
    python $INFER_TEST >$CLIENT_LOG 2>&1
    if [ $? -ne 0 ]; then
        cat $CLIENT_LOG
        echo -e "\n***\n*** Test Failed\n***"
        RET=1
    fi

    grep -c "HTTP/1.1 200 OK" $CLIENT_LOG
    if [ $? -ne 0 ]; then
        cat $CLIENT_LOG
        echo -e "\n***\n*** Test Failed To Run\n***"
        RET=1
    fi

    set -e

    kill $SERVER_PID
    wait $SERVER_PID
done

if [ $RET -eq 0 ]; then
  echo -e "\n***\n*** Test Passed\n***"
fi

exit $RET
