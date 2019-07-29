# Copyright (c) 2019, NVIDIA CORPORATION. All rights reserved.
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

import sys
sys.path.append("../common")

from builtins import range
from future.utils import iteritems
import unittest
import numpy as np
import infer_util as iu
import test_util as tu
from tensorrtserver.api import *

np_dtype_string = np.dtype(object)

class InferVariableTest(unittest.TestCase):
    def _full_exact(self, input_dtype, output0_dtype, output1_dtype,
                    input_shape, output0_shape, output1_shape,
                    output0_raw=True, output1_raw=True, swap=False):
        def _infer_exact_helper(tester, pf, tensor_shape, batch_size,
                input_dtype, output0_dtype, output1_dtype,
                output0_raw=True, output1_raw=True,
                model_version=None, swap=False,
                outputs=("OUTPUT0", "OUTPUT1"), use_http=True, use_grpc=True,
                skip_request_id_check=False, use_streaming=True,
                correlation_id=0):
            for bs in (1, batch_size):
                # model that does not support batching
                if bs == 1:
                    iu.infer_exact(tester, pf + "_nobatch", tensor_shape, bs,
                                    input_dtype, output0_dtype, output1_dtype,
                                    output0_raw, output1_raw,
                                    model_version, swap,
                                    outputs, use_http, use_grpc,
                                    skip_request_id_check, use_streaming,
                                    correlation_id)
                # model that supports batching
                iu.infer_exact(tester, pf, tensor_shape, bs,
                               input_dtype, output0_dtype, output1_dtype,
                               output0_raw, output1_raw,
                               model_version, swap,
                               outputs, use_http, use_grpc,
                               skip_request_id_check, use_streaming,
                               correlation_id)

        all_ensemble_prefix = ["simple_", "sequence_", "fan_"]
        ensemble_prefix = [""]
        for prefix in all_ensemble_prefix:
            if tu.validate_for_ensemble_model(prefix,
                                    input_dtype, output0_dtype, output1_dtype,
                                    input_shape, input_shape, input_shape):
                ensemble_prefix.append(prefix)

        if tu.validate_for_tf_model(input_dtype, output0_dtype, output1_dtype,
                                    input_shape, output0_shape, output1_shape):
            for prefix in ensemble_prefix:
                for pf in ["graphdef", "savedmodel"]:
                    _infer_exact_helper(self, prefix + pf, input_shape, 8,
                                    input_dtype, output0_dtype, output1_dtype,
                                    output0_raw=output0_raw, output1_raw=output1_raw, swap=swap)

        if tu.validate_for_c2_model(input_dtype, output0_dtype, output1_dtype,
                                    input_shape, output0_shape, output1_shape):
            for prefix in ensemble_prefix:
                _infer_exact_helper(self, prefix + 'netdef', input_shape, 8,
                                input_dtype, output0_dtype, output1_dtype,
                                output0_raw=output0_raw, output1_raw=output1_raw, swap=swap)

        # the custom model is src/custom/addsub... it does not swap
        # the inputs so always set to False
        if tu.validate_for_custom_model(input_dtype, output0_dtype, output1_dtype,
                                        input_shape, output0_shape, output1_shape):
            # No basic ensemble models are created against custom models
            _infer_exact_helper(self, 'custom', input_shape, 8,
                            input_dtype, output0_dtype, output1_dtype,
                            output0_raw=output0_raw, output1_raw=output1_raw, swap=False)

        if tu.validate_for_onnx_model(input_dtype, output0_dtype, output1_dtype,
                                    input_shape, output0_shape, output1_shape):
            # No basic ensemble models are created against custom models [TODO]
            _infer_exact_helper(self, 'onnx', input_shape, 8,
                            input_dtype, output0_dtype, output1_dtype,
                            output0_raw=output0_raw, output1_raw=output1_raw, swap=swap)

    def test_raw_fff(self):
        self._full_exact(np.float32, np.float32, np.float32, (16,), (16,), (16,))
    def test_raw_fii(self):
        self._full_exact(np.float32, np.int32, np.int32, (2,8), (2,8), (2,8))
    def test_raw_fll(self):
        self._full_exact(np.float32, np.int64, np.int64, (8,4), (8,4), (8,4))
    def test_raw_fil(self):
        self._full_exact(np.float32, np.int32, np.int64, (2,8,2), (2,8,2), (2,8,2))
    def test_raw_ffi(self):
        self._full_exact(np.float32, np.float32, np.int32, (16,), (16,), (16,))
    def test_raw_iii(self):
        self._full_exact(np.int32, np.int32, np.int32, (2,8), (2,8), (2,8))
    def test_faw_iif(self):
        self._full_exact(np.int32, np.int32, np.float32, (2,8,2), (2,8,2), (2,8,2))

    def test_raw_ooo(self):
        self._full_exact(np_dtype_string, np_dtype_string, np_dtype_string, (16,), (16,), (16,))
    def test_raw_oii(self):
        self._full_exact(np_dtype_string, np.int32, np.int32, (2,8), (2,8), (2,8))
    def test_raw_ooi(self):
        self._full_exact(np_dtype_string, np_dtype_string, np.int32, (8,4), (8,4), (8,4))
    def test_raw_oio(self):
        self._full_exact(np_dtype_string, np.int32, np_dtype_string, (2,8,2), (2,8,2), (2,8,2))

    def test_class_fff(self):
        self._full_exact(np.float32, np.float32, np.float32, (16,), (16,), (16,),
                         output0_raw=False, output1_raw=False)
    def test_class_fii(self):
        self._full_exact(np.float32, np.int32, np.int32, (2,8), (2,8), (2,8),
                         output0_raw=False, output1_raw=False)
    def test_class_fll(self):
        self._full_exact(np.float32, np.int64, np.int64, (8,4), (8,4), (8,4),
                         output0_raw=False, output1_raw=False)
    def test_class_fil(self):
        self._full_exact(np.float32, np.int32, np.int64, (2,8,2), (2,8,2), (2,8,2),
                         output0_raw=False, output1_raw=False)

    def test_class_ffi(self):
        self._full_exact(np.float32, np.float32, np.int32, (16,), (16,), (16,),
                         output0_raw=False, output1_raw=False)
    def test_class_iii(self):
        self._full_exact(np.int32, np.int32, np.int32, (2,8), (2,8), (2,8),
                         output0_raw=False, output1_raw=False)
    def test_class_iif(self):
        self._full_exact(np.int32, np.int32, np.float32, (2,8,2), (2,8,2), (2,8,2),
                         output0_raw=False, output1_raw=False)

    def test_mix_ffi(self):
        self._full_exact(np.float32, np.float32, np.int32, (16,), (16,), (16,),
                         output0_raw=True, output1_raw=False)
    def test_mix_iii(self):
        self._full_exact(np.int32, np.int32, np.int32, (2,8), (2,8), (2,8),
                         output0_raw=False, output1_raw=True)
    def test_mix_iif(self):
        self._full_exact(np.int32, np.int32, np.float32, (2,8,2), (2,8,2), (2,8,2),
                         output0_raw=True, output1_raw=False)


if __name__ == '__main__':
    unittest.main()
