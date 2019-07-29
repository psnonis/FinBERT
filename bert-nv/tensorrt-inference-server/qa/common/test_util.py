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

import numpy as np

_last_request_id = 0

def shape_element_count(shape):
    cnt = 0
    for d in shape:
        if d == -1:
            return -1
        if cnt == 0:
            cnt = d
        else:
            cnt = cnt * d
    return cnt

def shape_is_fixed(shape):
    return shape_element_count(shape) != -1

def shape_to_tf_shape(shape):
    return [None if i == -1 else i for i in shape]

def shape_to_onnx_shape(shape, idx=0, increment_index=True):
    # Onnx use string for variable size dimension, and the same string
    # will be inferred to have same value for the model run.
    # So there is an extra "idx" parameter to make sure the string is
    # unique
    res = []
    for dim in shape:
        if dim == -1:
            res.append("var_" + str(idx))
            if increment_index:
                idx += 1
        else:
            res.append(dim)
    return res, idx

def shape_to_dims_str(shape):
    return ','.join(str(i) for i in shape)

def validate_for_tf_model(input_dtype, output0_dtype, output1_dtype,
                          input_shape, output0_shape, output1_shape):
    """Return True if input and output dtypes are supported by a TF model."""

    # If the input type is string the output type must be string or
    # int32. This is because the QA models we generate convert strings
    # internally to int32 for compute.
    if ((input_dtype == np.object) and
        (((output0_dtype != np.object) and (output0_dtype != np.int32)) or
         ((output1_dtype != np.object) and (output1_dtype != np.int32)))):
        return False

    return True

def validate_for_c2_model(input_dtype, output0_dtype, output1_dtype,
                          input_shape, output0_shape, output1_shape):
    """Return True if input and output dtypes are supported by a Caffe2 model."""

    # Some operations used by test don't support fp16.
    if ((input_dtype == np.float16) or (output0_dtype == np.float16) or
        (output1_dtype == np.float16)):
        return False

    # Some operations don't support any int type except int32.
    if ((input_dtype == np.int8) or (output0_dtype == np.int8) or
        (output1_dtype == np.int8) or (input_dtype == np.int16) or
        (output0_dtype == np.int16) or (output1_dtype == np.int16)):
        return False

    # If the input type is string the output type must be string or
    # int32. This is because the QA models we generate convert strings
    # internally to int32 for compute.
    if ((input_dtype == np.object) and
        (((output0_dtype != np.object) and (output0_dtype != np.int32)) or
         ((output1_dtype != np.object) and (output1_dtype != np.int32)))):
        return False

    # Don't support string inputs or outputs.
    if ((input_dtype == np.object) or (output0_dtype == np.object) or
        (output1_dtype == np.object)):
        return False

    return True

def validate_for_trt_model(input_dtype, output0_dtype, output1_dtype,
                           input_shape, output0_shape, output1_shape):
    """Return True if input and output dtypes are supported by a TRT model."""

    # TRT supports limited datatypes as of TRT 5.0. Input can be FP16 or
    # FP32, output must be FP32.
    if (input_dtype != np.float16) and (input_dtype != np.float32):
        return False
    if (output0_dtype != np.float32) or (output1_dtype != np.float32):
        return False

    # Input and output shapes must be fixed-size.
    if (not shape_is_fixed(input_shape) or
        not shape_is_fixed(output0_shape) or
        not shape_is_fixed(output1_shape)):
        return False

    return True

def validate_for_custom_model(input_dtype, output0_dtype, output1_dtype,
                              input_shape, output0_shape, output1_shape):
    """Return True if input and output dtypes are supported by custom model."""

    # The custom model is src/custom/addsub... it only supports int32
    # and fp32, and input and output datatype must be equal.
    if (input_dtype != np.int32) and (input_dtype != np.float32):
        return False
    if (output0_dtype != input_dtype) or (output1_dtype != input_dtype):
        return False

    # Input and output shapes must be fixed-size.
    if (not shape_is_fixed(input_shape) or
        not shape_is_fixed(output0_shape) or
        not shape_is_fixed(output1_shape)):
        return False

    return True

def validate_for_ensemble_model(ensemble_type,
                              input_dtype, output0_dtype, output1_dtype,
                              input_shape, output0_shape, output1_shape):
    """Return True if input and output dtypes are supported by the ensemble type."""

    # Those ensemble types contains "identity" model which doesn't allow STRING
    # data type
    # Test types that use identity for both input and output
    test_type_involved = ["reshape", "zero", "fan"]
    if input_dtype == np.object or output0_dtype == np.object or output1_dtype == np.object:
        for type_str in test_type_involved:
            if type_str in ensemble_type:
                return False
                
    # Otherwise, check input / output separately
    if input_dtype == np.object and "sequence" in ensemble_type:
        return False

    return True

def validate_for_onnx_model(input_dtype, output0_dtype, output1_dtype,
                           input_shape, output0_shape, output1_shape):
    """Return True if input and output dtypes are supported by a Onnx model."""

    # If the input type is string the output type must be string or
    # int32. This is because the QA models we generate convert strings
    # internally to int32 for compute.
    if ((input_dtype == np.object) and
        (((output0_dtype != np.object) and (output0_dtype != np.int32)) or
         ((output1_dtype != np.object) and (output1_dtype != np.int32)))):
        return False

    return True

def get_model_name(pf, input_dtype, output0_dtype, output1_dtype):
    return "{}_{}_{}_{}".format(
        pf, np.dtype(input_dtype).name, np.dtype(output0_dtype).name,
        np.dtype(output1_dtype).name)

def get_sequence_model_name(pf, dtype):
    return "{}_sequence_{}".format(pf, np.dtype(dtype).name)

def get_zero_model_name(pf, io_cnt, dtype):
    return "{}_zero_{}_{}".format(pf, io_cnt, np.dtype(dtype).name)
