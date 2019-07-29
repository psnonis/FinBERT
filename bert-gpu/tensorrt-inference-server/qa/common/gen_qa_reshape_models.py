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

import argparse
from builtins import range
import os
import sys
import numpy as np
import gen_ensemble_model_utils as emu

FLAGS = None
np_dtype_string = np.dtype(object)

def np_to_model_dtype(np_dtype):
    if np_dtype == np.bool:
        return "TYPE_BOOL"
    elif np_dtype == np.int8:
        return "TYPE_INT8"
    elif np_dtype == np.int16:
        return "TYPE_INT16"
    elif np_dtype == np.int32:
        return "TYPE_INT32"
    elif np_dtype == np.int64:
        return "TYPE_INT64"
    elif np_dtype == np.uint8:
        return "TYPE_UINT8"
    elif np_dtype == np.uint16:
        return "TYPE_UINT16"
    elif np_dtype == np.float16:
        return "TYPE_FP16"
    elif np_dtype == np.float32:
        return "TYPE_FP32"
    elif np_dtype == np.float64:
        return "TYPE_FP64"
    elif np_dtype == np_dtype_string:
        return "TYPE_STRING"
    return None

def np_to_tf_dtype(np_dtype):
    if np_dtype == np.bool:
        return tf.bool
    elif np_dtype == np.int8:
        return tf.int8
    elif np_dtype == np.int16:
        return tf.int16
    elif np_dtype == np.int32:
        return tf.int32
    elif np_dtype == np.int64:
        return tf.int64
    elif np_dtype == np.uint8:
        return tf.uint8
    elif np_dtype == np.uint16:
        return tf.uint16
    elif np_dtype == np.float16:
        return tf.float16
    elif np_dtype == np.float32:
        return tf.float32
    elif np_dtype == np.float64:
        return tf.float64
    elif np_dtype == np_dtype_string:
        return tf.string
    return None

def np_to_c2_dtype(np_dtype):
    if np_dtype == np.bool:
        return c2core.DataType.BOOL
    elif np_dtype == np.int8:
        return c2core.DataType.INT8
    elif np_dtype == np.int16:
        return c2core.DataType.INT16
    elif np_dtype == np.int32:
        return c2core.DataType.INT32
    elif np_dtype == np.int64:
        return c2core.DataType.INT64
    elif np_dtype == np.uint8:
        return c2core.DataType.UINT8
    elif np_dtype == np.uint16:
        return c2core.DataType.UINT16
    elif np_dtype == np.float16:
        return c2core.DataType.FLOAT16
    elif np_dtype == np.float32:
        return c2core.DataType.FLOAT
    elif np_dtype == np.float64:
        return c2core.DataType.DOUBLE
    elif np_dtype == np_dtype_string:
        return c2core.DataType.STRING
    return None

def np_to_trt_dtype(np_dtype):
    if np_dtype == np.int8:
        return trt.infer.DataType.INT8
    elif np_dtype == np.int32:
        return trt.infer.DataType.INT32
    elif np_dtype == np.float16:
        return trt.infer.DataType.HALF
    elif np_dtype == np.float32:
        return trt.infer.DataType.FLOAT
    return None

def np_to_onnx_dtype(np_dtype):
    if np_dtype == np.bool:
        return onnx.TensorProto.BOOL
    elif np_dtype == np.int8:
        return onnx.TensorProto.INT8
    elif np_dtype == np.int16:
        return onnx.TensorProto.INT16
    elif np_dtype == np.int32:
        return onnx.TensorProto.INT32
    elif np_dtype == np.int64:
        return onnx.TensorProto.INT64
    elif np_dtype == np.uint8:
        return onnx.TensorProto.UINT8
    elif np_dtype == np.uint16:
        return onnx.TensorProto.UINT16
    elif np_dtype == np.float16:
        return onnx.TensorProto.FLOAT16
    elif np_dtype == np.float32:
        return onnx.TensorProto.FLOAT
    elif np_dtype == np.float64:
        return onnx.TensorProto.DOUBLE
    elif np_dtype == np_dtype_string:
        return onnx.TensorProto.STRING
    return None

def create_tf_modelfile(
        create_savedmodel, models_dir, model_version, max_batch,
        dtype, input_shapes, output_shapes):

    assert len(input_shapes) == len(output_shapes)
    if not tu.validate_for_tf_model(dtype, dtype, dtype,
                                    input_shapes[0], input_shapes[0], input_shapes[0]):
        return

    tf_dtype = np_to_tf_dtype(dtype)
    io_cnt = len(input_shapes)

    # Create the model that copies inputs to corresponding outputs.
    tf.reset_default_graph()
    for io_num in range(io_cnt):
        input_name = "INPUT{}".format(io_num)
        output_name = "OUTPUT{}".format(io_num)
        if max_batch == 0:
            tin = tf.placeholder(tf_dtype, tu.shape_to_tf_shape(input_shapes[io_num]), input_name)
        else:
            tin = tf.placeholder(
                tf_dtype, [None,] + tu.shape_to_tf_shape(input_shapes[io_num]), input_name)

        if input_shapes == output_shapes:
            toutput = tf.identity(tin, name=output_name)
        else:
            if max_batch == 0:
                toutput = tf.reshape(tin, output_shapes[io_num], name=output_name)
            else:
                toutput = tf.reshape(tin, [-1,] + output_shapes[io_num], name=output_name)

    # Use model name based on input/output count and non-batching variant
    if create_savedmodel:
        model_name = tu.get_zero_model_name(
            "savedmodel_nobatch" if max_batch == 0 else "savedmodel", io_cnt, dtype)
    else:
        model_name = tu.get_zero_model_name(
            "graphdef_nobatch" if max_batch == 0 else "graphdef", io_cnt, dtype)

    model_version_dir = models_dir + "/" + model_name + "/" + str(model_version)

    try:
        os.makedirs(model_version_dir)
    except OSError as ex:
        pass # ignore existing dir

    if create_savedmodel:
        with tf.Session() as sess:
            input_dict = {}
            output_dict = {}
            for io_num in range(io_cnt):
                input_name = "INPUT{}".format(io_num)
                output_name = "OUTPUT{}".format(io_num)
                input_tensor = tf.get_default_graph().get_tensor_by_name(input_name + ":0")
                output_tensor = tf.get_default_graph().get_tensor_by_name(output_name + ":0")
                input_dict[input_name] = input_tensor
                output_dict[output_name] = output_tensor
            tf.saved_model.simple_save(sess, model_version_dir + "/model.savedmodel",
                                       inputs=input_dict, outputs=output_dict)
    else:
        with tf.Session() as sess:
            graph_io.write_graph(sess.graph.as_graph_def(), model_version_dir,
                                 "model.graphdef", as_text=False)

def create_tf_modelconfig(
        create_savedmodel, models_dir, model_version, max_batch, dtype,
        input_shapes, input_model_shapes, output_shapes, output_model_shapes):

    assert len(input_shapes) == len(input_model_shapes)
    assert len(output_shapes) == len(output_model_shapes)
    assert len(input_shapes) == len(output_shapes)
    if not tu.validate_for_tf_model(dtype, dtype, dtype,
                                    input_shapes[0], input_shapes[0], input_shapes[0]):
        return

    io_cnt = len(input_shapes)

    # Use a different model name for the non-batching variant
    if create_savedmodel:
        model_name = tu.get_zero_model_name(
            "savedmodel_nobatch" if max_batch == 0 else "savedmodel", io_cnt, dtype)
    else:
        model_name = tu.get_zero_model_name(
            "graphdef_nobatch" if max_batch == 0 else "graphdef", io_cnt, dtype)

    config_dir = models_dir + "/" + model_name
    config = '''
name: "{}"
platform: "{}"
max_batch_size: {}
'''.format(model_name,
           "tensorflow_savedmodel" if create_savedmodel else "tensorflow_graphdef",
           max_batch)

    for io_num in range(io_cnt):
        config += '''
input [
  {{
    name: "INPUT{}"
    data_type: {}
    dims: [ {} ]
    {}
  }}
]
output [
  {{
    name: "OUTPUT{}"
    data_type: {}
    dims: [ {} ]
    {}
  }}
]
'''.format(io_num, np_to_model_dtype(dtype),
           tu.shape_to_dims_str(input_shapes[io_num]),
           "reshape: {{ shape: [ {} ] }}".format(
               tu.shape_to_dims_str(input_model_shapes[io_num]))
               if input_shapes[io_num] != input_model_shapes[io_num] else "",
           io_num, np_to_model_dtype(dtype),
           tu.shape_to_dims_str(output_shapes[io_num]),
           "reshape: {{ shape: [ {} ] }}".format(
               tu.shape_to_dims_str(output_model_shapes[io_num]))
               if output_shapes[io_num] != output_model_shapes[io_num] else "")

    try:
        os.makedirs(config_dir)
    except OSError as ex:
        pass # ignore existing dir

    with open(config_dir + "/config.pbtxt", "w") as cfile:
        cfile.write(config)


def create_netdef_modelfile(
        models_dir, model_version, max_batch,
        dtype, input_shapes, output_shapes):

    assert len(input_shapes) == len(output_shapes)
    if not tu.validate_for_c2_model(dtype, dtype, dtype,
                                    input_shapes[0], input_shapes[0], input_shapes[0]):
        return

    c2_dtype = np_to_c2_dtype(dtype)
    io_cnt = len(input_shapes)

    model_name = tu.get_zero_model_name(
        "netdef_nobatch" if max_batch == 0 else "netdef", io_cnt, dtype)

    # Create the model that copies inputs to corresponding outputs.
    model = c2model_helper.ModelHelper(name=model_name)
    for io_num in range(io_cnt):
        if input_shapes == output_shapes:
            model.net.Copy("INPUT{}".format(io_num), "OUTPUT{}".format(io_num))
        else:
            if max_batch == 0:
                old_shape = input_shapes[io_num]
                model.net.Reshape(["INPUT{}".format(io_num)], ["OUTPUT{}".format(io_num), 'old_shape'],
                                  shape=output_shapes[io_num])
            else:
                old_shape = [-1] + input_shapes[io_num]
                model.net.Reshape(["INPUT{}".format(io_num)], ["OUTPUT{}".format(io_num), 'old_shape'],
                                  shape=([-1] + output_shapes[io_num]))

    model_version_dir = models_dir + "/" + model_name + "/" + str(model_version)

    try:
        os.makedirs(model_version_dir)
    except OSError as ex:
        pass # ignore existing dir

    with open(model_version_dir + "/model.netdef", "wb") as f:
        f.write(model.Proto().SerializeToString())
    with open(model_version_dir + "/init_model.netdef", "wb") as f:
        f.write(model.InitProto().SerializeToString())


def create_netdef_modelconfig(
        models_dir, model_version, max_batch, dtype,
        input_shapes, input_model_shapes, output_shapes, output_model_shapes):

    assert len(input_shapes) == len(input_model_shapes)
    assert len(output_shapes) == len(output_model_shapes)
    assert len(input_shapes) == len(output_shapes)
    if not tu.validate_for_c2_model(dtype, dtype, dtype,
                                    input_shapes[0], input_shapes[0], input_shapes[0]):
        return

    io_cnt = len(input_shapes)

    model_name = tu.get_zero_model_name(
        "netdef_nobatch" if max_batch == 0 else "netdef", io_cnt, dtype)
    config_dir = models_dir + "/" + model_name
    config = '''
name: "{}"
platform: "caffe2_netdef"
max_batch_size: {}
'''.format(model_name, max_batch)

    for io_num in range(io_cnt):
        config += '''
input [
  {{
    name: "INPUT{}"
    data_type: {}
    dims: [ {} ]
    {}
  }}
]
output [
  {{
    name: "OUTPUT{}"
    data_type: {}
    dims: [ {} ]
    {}
  }}
]
'''.format(io_num, np_to_model_dtype(dtype),
           tu.shape_to_dims_str(input_shapes[io_num]),
           "reshape: {{ shape: [ {} ] }}".format(
               tu.shape_to_dims_str(input_model_shapes[io_num]))
               if input_shapes[io_num] != input_model_shapes[io_num] else "",
           io_num, np_to_model_dtype(dtype),
           tu.shape_to_dims_str(output_shapes[io_num]),
           "reshape: {{ shape: [ {} ] }}".format(
               tu.shape_to_dims_str(output_model_shapes[io_num]))
               if output_shapes[io_num] != output_model_shapes[io_num] else "")

    try:
        os.makedirs(config_dir)
    except OSError as ex:
        pass # ignore existing dir

    with open(config_dir + "/config.pbtxt", "w") as cfile:
        cfile.write(config)


def create_plan_modelfile(
        models_dir, model_version, max_batch,
        dtype, input_shapes, output_shapes):

    assert len(input_shapes) == len(output_shapes)
    if not tu.validate_for_trt_model(dtype, dtype, dtype,
                                     input_shapes[0], input_shapes[0], input_shapes[0]):
        return

    trt_dtype = np_to_trt_dtype(dtype)
    io_cnt = len(input_shapes)

    # Create the model that copies inputs to corresponding outputs.
    G_LOGGER = trt.infer.ConsoleLogger(trt.infer.LogSeverity.INFO)
    builder = trt.infer.create_infer_builder(G_LOGGER)
    network = builder.create_network()

    for io_num in range(io_cnt):
        input_name = "INPUT{}".format(io_num)
        output_name = "OUTPUT{}".format(io_num)
        in0 = network.add_input(input_name, trt_dtype, input_shapes[io_num])
        if input_shapes == output_shapes:
            out0 = network.add_identity(in0)
        else:
            out0 = network.add_shuffle(in0)
            out0.set_reshape_dimensions(output_shapes[io_num])

        out0.get_output(0).set_name(output_name)
        network.mark_output(out0.get_output(0))

    builder.set_max_batch_size(max(1, max_batch))
    builder.set_max_workspace_size(1 << 20)
    engine = builder.build_cuda_engine(network)
    network.destroy()

    model_name = tu.get_zero_model_name(
        "plan_nobatch" if max_batch == 0 else "plan", io_cnt, dtype)
    model_version_dir = models_dir + "/" + model_name + "/" + str(model_version)

    try:
        os.makedirs(model_version_dir)
    except OSError as ex:
        pass # ignore existing dir

    lengine = trt.lite.Engine(engine_stream=engine.serialize(),
                              max_batch_size=max(1, max_batch))
    lengine.save(model_version_dir + "/model.plan")
    engine.destroy()
    builder.destroy()


def create_plan_modelconfig(
        models_dir, model_version, max_batch, dtype,
        input_shapes, input_model_shapes, output_shapes, output_model_shapes):

    assert len(input_shapes) == len(input_model_shapes)
    assert len(output_shapes) == len(output_model_shapes)
    assert len(input_shapes) == len(output_shapes)
    if not tu.validate_for_trt_model(dtype, dtype, dtype,
                                     input_shapes[0], input_shapes[0], input_shapes[0]):
        return

    io_cnt = len(input_shapes)

    model_name = tu.get_zero_model_name(
        "plan_nobatch" if max_batch == 0 else "plan", io_cnt, dtype)
    config_dir = models_dir + "/" + model_name
    config = '''
name: "{}"
platform: "tensorrt_plan"
max_batch_size: {}
'''.format(model_name, max_batch)

    for io_num in range(io_cnt):
        config += '''
input [
  {{
    name: "INPUT{}"
    data_type: {}
    dims: [ {} ]
    {}
  }}
]
output [
  {{
    name: "OUTPUT{}"
    data_type: {}
    dims: [ {} ]
    {}
  }}
]
'''.format(io_num, np_to_model_dtype(dtype),
           tu.shape_to_dims_str(input_shapes[io_num]),
           "reshape: {{ shape: [ {} ] }}".format(
               tu.shape_to_dims_str(input_model_shapes[io_num]))
               if input_shapes[io_num] != input_model_shapes[io_num] else "",
           io_num, np_to_model_dtype(dtype),
           tu.shape_to_dims_str(output_shapes[io_num]),
           "reshape: {{ shape: [ {} ] }}".format(
               tu.shape_to_dims_str(output_model_shapes[io_num]))
               if output_shapes[io_num] != output_model_shapes[io_num] else "")

    try:
        os.makedirs(config_dir)
    except OSError as ex:
        pass # ignore existing dir

    with open(config_dir + "/config.pbtxt", "w") as cfile:
        cfile.write(config)

def create_ensemble_modelfile(
        models_dir, model_version, max_batch,
        dtype, input_shapes, output_shapes):

    assert len(input_shapes) == len(output_shapes)
    if not tu.validate_for_ensemble_model("reshape", dtype, dtype, dtype,
                                    input_shapes[0], input_shapes[0], input_shapes[0]):
        return

    emu.create_identity_ensemble_modelfile(
        "reshape", models_dir, model_version, max_batch,
        dtype, input_shapes, output_shapes)


def create_ensemble_modelconfig(
        models_dir, model_version, max_batch, dtype,
        input_shapes, input_model_shapes, output_shapes, output_model_shapes):

    assert len(input_shapes) == len(input_model_shapes)
    assert len(output_shapes) == len(output_model_shapes)
    assert len(input_shapes) == len(output_shapes)
    if not tu.validate_for_ensemble_model("reshape", dtype, dtype, dtype,
                                    input_shapes[0], input_shapes[0], input_shapes[0]):
        return

    emu.create_identity_ensemble_modelconfig(
        "reshape", models_dir, model_version, max_batch, dtype,
        input_shapes, input_model_shapes, output_shapes, output_model_shapes)


def create_onnx_modelfile(
        models_dir, model_version, max_batch,
        dtype, input_shapes, output_shapes):

    assert len(input_shapes) == len(output_shapes)
    if not tu.validate_for_onnx_model(dtype, dtype, dtype,
                                      input_shapes[0], input_shapes[0], input_shapes[0]):
        return

    onnx_dtype = np_to_onnx_dtype(dtype)
    io_cnt = len(input_shapes)

    # Create the model
    model_name = tu.get_zero_model_name("onnx_nobatch" if max_batch == 0 else "onnx",
                                   io_cnt, dtype)
    model_version_dir = models_dir + "/" + model_name + "/" + str(model_version)

    batch_dim = [] if max_batch == 0 else [max_batch]

    onnx_nodes = []
    onnx_inputs = []
    onnx_outputs = []
    idx = 0
    for io_num in range(io_cnt):
        # Repeat so that the variable dimension name is different
        in_shape, idx = tu.shape_to_onnx_shape(input_shapes[io_num], idx)
        out_shape, idx = tu.shape_to_onnx_shape(output_shapes[io_num], idx)
        in_name = "INPUT{}".format(io_num)
        out_name = "OUTPUT{}".format(io_num)
        out_shape_name = out_name + "_shape"

        onnx_inputs.append(onnx.helper.make_tensor_value_info(in_name, onnx_dtype, batch_dim + in_shape))
        onnx_outputs.append(onnx.helper.make_tensor_value_info(out_name, onnx_dtype, batch_dim + out_shape))
        
        if input_shapes == output_shapes:
            onnx_nodes.append(onnx.helper.make_node("Identity", [in_name], [out_name]))
        else:
            onnx_nodes.append(onnx.helper.make_node("Shape", [out_name], [out_shape_name]))
            onnx_nodes.append(onnx.helper.make_node("Reshape", [in_name, out_shape_name], [out_name]))

    graph_proto = onnx.helper.make_graph(onnx_nodes, model_name, onnx_inputs, onnx_outputs)
    model_def = onnx.helper.make_model(graph_proto, producer_name="TRTIS")

    try:
        os.makedirs(model_version_dir)
    except OSError as ex:
        pass # ignore existing dir

    onnx.save(model_def, model_version_dir + "/model.onnx")


def create_onnx_modelconfig(
        models_dir, model_version, max_batch, dtype,
        input_shapes, input_model_shapes, output_shapes, output_model_shapes):

    assert len(input_shapes) == len(input_model_shapes)
    assert len(output_shapes) == len(output_model_shapes)
    assert len(input_shapes) == len(output_shapes)
    if not tu.validate_for_onnx_model(dtype, dtype, dtype,
                                      input_shapes[0], input_shapes[0], input_shapes[0]):
        return

    io_cnt = len(input_shapes)

    # Use a different model name for the non-batching variant
    model_name = tu.get_zero_model_name("onnx_nobatch" if max_batch == 0 else "onnx",
                                   io_cnt, dtype)
    config_dir = models_dir + "/" + model_name
    
    config = emu.create_general_modelconfig(model_name, "onnxruntime_onnx", max_batch,
            emu.repeat(dtype, io_cnt), input_shapes, input_model_shapes,
            emu.repeat(dtype, io_cnt), output_shapes, output_model_shapes,
            emu.repeat(None, io_cnt), force_tensor_number_suffix=True)

    try:
        os.makedirs(config_dir)
    except OSError as ex:
        pass # ignore existing dir

    with open(config_dir + "/config.pbtxt", "w") as cfile:
        cfile.write(config)
    

def create_models(models_dir, dtype, input_shapes, input_model_shapes,
                  output_shapes=None, output_model_shapes=None, no_batch=True):
    model_version = 1
    if output_shapes is None:
        output_shapes = input_shapes
    if output_model_shapes is None:
        output_model_shapes = input_model_shapes

    if FLAGS.graphdef:
        create_tf_modelconfig(False, models_dir, model_version, 8, dtype,
                              input_shapes, input_model_shapes, output_shapes, output_model_shapes)
        create_tf_modelfile(False, models_dir, model_version, 8, dtype,
                            input_model_shapes, output_model_shapes)
        if no_batch:
            create_tf_modelconfig(False, models_dir, model_version, 0, dtype,
                                  input_shapes, input_model_shapes, output_shapes, output_model_shapes)
            create_tf_modelfile(False, models_dir, model_version, 0, dtype,
                                input_model_shapes, output_model_shapes)

    if FLAGS.savedmodel:
        create_tf_modelconfig(True, models_dir, model_version, 8, dtype,
                              input_shapes, input_model_shapes, output_shapes, output_model_shapes)
        create_tf_modelfile(True, models_dir, model_version, 8, dtype,
                            input_model_shapes, output_model_shapes)
        if no_batch:
            create_tf_modelconfig(True, models_dir, model_version, 0, dtype,
                                  input_shapes, input_model_shapes, output_shapes, output_model_shapes)
            create_tf_modelfile(True, models_dir, model_version, 0, dtype,
                                input_model_shapes, output_model_shapes)

    if FLAGS.netdef:
        create_netdef_modelconfig(models_dir, model_version, 8, dtype,
                                  input_shapes, input_model_shapes, output_shapes, output_model_shapes)
        create_netdef_modelfile(models_dir, model_version, 8, dtype,
                                input_model_shapes, output_model_shapes)
        if no_batch:
            create_netdef_modelconfig(models_dir, model_version, 0, dtype,
                                      input_shapes, input_model_shapes, output_shapes, output_model_shapes)
            create_netdef_modelfile(models_dir, model_version, 0, dtype,
                                    input_model_shapes, output_model_shapes)

    if FLAGS.onnx:
        create_onnx_modelconfig(models_dir, model_version, 8, dtype,
                                  input_shapes, input_model_shapes, output_shapes, output_model_shapes)
        create_onnx_modelfile(models_dir, model_version, 8, dtype,
                                input_model_shapes, output_model_shapes)
        if no_batch:
            create_onnx_modelconfig(models_dir, model_version, 0, dtype,
                                      input_shapes, input_model_shapes, output_shapes, output_model_shapes)
            create_onnx_modelfile(models_dir, model_version, 0, dtype,
                                    input_model_shapes, output_model_shapes)

    # Shouldn't create ensembles that reshape to zero-sized tensors. Reshaping
    # from / to zero dimension is not allow as ensemble inputs / outputs
    # are passed from / to other model AS IF direct inference from client.
    # But create it anyway, expecting that the ensemble models can be served but
    # they will always return error message.
    if FLAGS.ensemble:
        # Create fixed size nop for ensemble models
        for shape in input_model_shapes:
            emu.create_nop_modelconfig(models_dir, shape, np.float32)
            emu.create_nop_tunnel_modelconfig(models_dir, shape, np.float32)
            emu.create_nop_modelconfig(models_dir, [-1], np.float32)
        create_ensemble_modelconfig(models_dir, model_version, 8, dtype,
                                  input_shapes, input_model_shapes, output_shapes, output_model_shapes)
        create_ensemble_modelfile(models_dir, model_version, 8, dtype,
                                input_model_shapes, output_model_shapes)
        if no_batch:
            create_ensemble_modelconfig(models_dir, model_version, 0, dtype,
                                      input_shapes, input_model_shapes, output_shapes, output_model_shapes)
            create_ensemble_modelfile(models_dir, model_version, 0, dtype,
                                    input_model_shapes, output_model_shapes)


def create_trt_models(models_dir, dtype, input_shapes, input_model_shapes,
                      output_shapes=None, output_model_shapes=None, no_batch=True):
    model_version = 1
    if output_shapes is None:
        output_shapes = input_shapes
    if output_model_shapes is None:
        output_model_shapes = input_model_shapes

    if FLAGS.tensorrt:
        create_plan_modelconfig(models_dir, model_version, 8, dtype,
                                input_shapes, input_model_shapes, output_shapes, output_model_shapes)
        create_plan_modelfile(models_dir, model_version, 8, dtype,
                              input_model_shapes, output_model_shapes)
        if no_batch:
            create_plan_modelconfig(models_dir, model_version, 0, dtype,
                                    input_shapes, input_model_shapes, output_shapes, output_model_shapes)
            create_plan_modelfile(models_dir, model_version, 0, dtype,
                                  input_model_shapes, output_model_shapes)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--models_dir', type=str, required=True,
                        help='Top-level model directory')
    parser.add_argument('--graphdef', required=False, action='store_true',
                        help='Generate GraphDef models')
    parser.add_argument('--savedmodel', required=False, action='store_true',
                        help='Generate SavedModel models')
    parser.add_argument('--netdef', required=False, action='store_true',
                        help='Generate NetDef models')
    parser.add_argument('--tensorrt', required=False, action='store_true',
                        help='Generate TensorRT PLAN models')
    parser.add_argument('--onnx', required=False, action='store_true',
                        help='Generate Onnx Runtime Onnx models')
    parser.add_argument('--ensemble', required=False, action='store_true',
                        help='Generate ensemble models')
    FLAGS, unparsed = parser.parse_known_args()

    if FLAGS.netdef:
        from caffe2.python import core as c2core
        from caffe2.python import model_helper as c2model_helper
    if FLAGS.graphdef or FLAGS.savedmodel:
        import tensorflow as tf
        from tensorflow.python.framework import graph_io, graph_util
    if FLAGS.tensorrt:
        import tensorrt.legacy as trt
    if FLAGS.onnx:
        import onnx

    import test_util as tu

    # TensorRT must be handled separately since it doesn't support
    # zero-sized tensors.
    create_models(FLAGS.models_dir, np.float32, ([1],), ([],), no_batch=False)
    create_models(FLAGS.models_dir, np.float32, ([1], [8]), ([], [4,1,2]), no_batch=False)
    create_models(FLAGS.models_dir, np.float32, ([4,4], [2], [2,2,3]), ([16], [1,2], [3,2,2]))
    create_trt_models(FLAGS.models_dir, np.float32, ([1], [8]), ([1,1,1], [4,1,2]))

    # Models that reshape only the input, not the output.
    create_models(FLAGS.models_dir, np.float32,
                  ([4,4], [2], [2,2,3], [1]), ([16], [1,2], [3,2,2], [1]),
                  output_shapes=([16], [1,2], [3,2,2], [1]),
                  output_model_shapes=([16], [1,2], [3,2,2], [1]))

    create_trt_models(FLAGS.models_dir, np.float32,
                      ([4,4], [2], [2,2,3], [1]), ([2,2,4], [1,2,1], [3,2,2], [1,1,1]),
                      output_shapes=([2,2,4], [1,2,1], [3,2,2], [1,1,1]),
                      output_model_shapes=([2,2,4], [1,2,1], [3,2,2], [1,1,1]))

    # TRT plan that reshapes neither input nor output. Needed for
    # L0_perflab_nomodel.
    create_trt_models(FLAGS.models_dir, np.float32,
                      ([1,1,1],), ([1,1,1],))
