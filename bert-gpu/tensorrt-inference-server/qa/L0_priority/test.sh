#!/bin/bash
# Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
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

DATADIR=/data/inferenceserver

SERVER=/opt/tensorrtserver/bin/trtserver
SERVER_ARGS=--model-store=`pwd`/models
SERVER_LOG="./inference_server.log"
source ../common/util.sh

rm -f ./*.log
rm -fr models && mkdir -p models && \
    cp -r $DATADIR/qa_model_repository/plan_float32_float32_float32 \
       models/plan_float32_float32_float32_def && \
    rm -fr models/plan_float32_float32_float32_def/2 && \
    rm -fr models/plan_float32_float32_float32_def/3 && \
    (cd models/plan_float32_float32_float32_def && \
            sed -i 's/^name: "plan_float32_float32_float32"/name: "plan_float32_float32_float32_def"/' \
                config.pbtxt) && \
    cp -r models/plan_float32_float32_float32_def models/plan_float32_float32_float32_max && \
    (cd models/plan_float32_float32_float32_max && \
            sed -i 's/^name: "plan_float32_float32_float32_def"/name: "plan_float32_float32_float32_max"/' \
                config.pbtxt && \
            echo "optimization { priority: PRIORITY_MAX }" >> config.pbtxt) && \
    cp -r models/plan_float32_float32_float32_def models/plan_float32_float32_float32_min && \
    (cd models/plan_float32_float32_float32_min && \
            sed -i 's/^name: "plan_float32_float32_float32_def"/name: "plan_float32_float32_float32_min"/' \
                config.pbtxt && \
            echo "optimization { priority: PRIORITY_MIN }" >> config.pbtxt)

run_server
if [ "$SERVER_PID" == "0" ]; then
    echo -e "\n***\n*** Failed to start $SERVER\n***"
    cat $SERVER_LOG
    exit 1
fi

RET=0

set +e

grep "plan_float32_float32_float32_min" $SERVER_LOG | grep "stream priority 0"
if [ $? -ne 0 ]; then
    echo -e "\n***\n*** Failed. Expected MIN priority 0\n***"
    RET=1
fi

grep "plan_float32_float32_float32_max" $SERVER_LOG | grep "stream priority -1"
if [ $? -ne 0 ]; then
    echo -e "\n***\n*** Failed. Expected MAX priority -1\n***"
    RET=1
fi

grep "plan_float32_float32_float32_def" $SERVER_LOG | grep "stream priority 0"
if [ $? -ne 0 ]; then
    echo -e "\n***\n*** Failed. Expected DEFAULT priority 0\n***"
    RET=1
fi

set -e

kill $SERVER_PID
wait $SERVER_PID

if [ $RET -eq 0 ]; then
    echo -e "\n***\n*** Test Passed\n***"
else
    echo -e "\n***\n*** Test FAILED\n***"
fi

exit $RET
