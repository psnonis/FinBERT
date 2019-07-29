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
        create_savedmodel, models_dir, model_version, max_batch, dtype, shape):

    if not tu.validate_for_tf_model(dtype, dtype, dtype, shape, shape, shape):
        return

    tf_input_dtype = np_to_tf_dtype(dtype)
    tf_dtype = tf_input_dtype

    # If the input is a string then use int32 for operation and just
    # cast to/from string for input and output.
    if tf_input_dtype == tf.string:
        tf_dtype = tf.int32

    # Create the model. If non-batching then don't include the batch
    # dimension.
    tf.reset_default_graph()
    if create_savedmodel and (max_batch == 0):
        input0 = tf.placeholder(tf_input_dtype, [1,], "INPUT")
        if tf_input_dtype == tf.string:
            input0 = tf.strings.to_number(tf.strings.join(["0", input0]), tf_dtype)
        start0 = tf.placeholder(tf_dtype, [1,], "START")
        ready0 = tf.placeholder(tf_dtype, [1,], "READY")
        acc = tf.get_variable("ACC", [1,], dtype=tf_dtype)
        tmp = tf.where(tf.equal(start0, 1), input0, tf.add(acc, input0))
        newacc = tf.where(tf.equal(ready0, 1), tmp, acc)
        assign = tf.assign(acc, newacc)
        if tf_input_dtype == tf.string:
            output0 = tf.dtypes.as_string(assign, name="OUTPUT")
        else:
            output0 = tf.identity(assign, name="OUTPUT")
    else:
        # For batching we can't use a tf.variable to hold the
        # accumulated values since that forces the size of the output
        # to the size of the variable (which must be a max-batch-size
        # vector since require one accumulator each), instead of the
        # output shape being [None, 1]. So instead we just return 0 if
        # not-ready and 'INPUT'+'START' otherwise... the tests know to
        # expect this.
        input0 = tf.placeholder(tf_input_dtype, [None,] + tu.shape_to_tf_shape(shape), "INPUT")
        if tf_input_dtype == tf.string:
            input0 = tf.strings.to_number(tf.strings.join(["0", input0]), tf_dtype)
        start0 = tf.placeholder(tf_dtype, [None,1], "START")
        ready0 = tf.placeholder(tf_dtype, [None,1], "READY")
        tmp = tf.where(tf.equal(ready0, 1), tf.add(start0, input0),
                       tf.zeros(tf.shape(input0), dtype=tf_dtype))
        if tf_input_dtype == tf.string:
            output0 = tf.dtypes.as_string(tmp, name="OUTPUT")
        else:
            output0 = tf.identity(tmp, name="OUTPUT")

    # Use a different model name for the non-batching variant
    if create_savedmodel:
        model_name = tu.get_sequence_model_name(
            "savedmodel_nobatch" if max_batch == 0 else "savedmodel", dtype)
    else:
        model_name = tu.get_sequence_model_name(
            "graphdef_nobatch" if max_batch == 0 else "graphdef", dtype)

    model_version_dir = models_dir + "/" + model_name + "/" + str(model_version)

    try:
        os.makedirs(model_version_dir)
    except OSError as ex:
        pass # ignore existing dir

    if create_savedmodel:
        with tf.Session() as sess:
            sess.run(tf.initializers.global_variables())
            input0_tensor = tf.get_default_graph().get_tensor_by_name("INPUT:0")
            start0_tensor = tf.get_default_graph().get_tensor_by_name("START:0")
            ready0_tensor = tf.get_default_graph().get_tensor_by_name("READY:0")
            output0_tensor = tf.get_default_graph().get_tensor_by_name("OUTPUT:0")
            tf.saved_model.simple_save(sess, model_version_dir + "/model.savedmodel",
                                       inputs={"INPUT": input0_tensor, "START": start0_tensor,
                                               "READY" : ready0_tensor},
                                       outputs={"OUTPUT": output0_tensor})
    else:
        with tf.Session() as sess:
            sess.run(tf.initializers.global_variables())
            graph_io.write_graph(sess.graph.as_graph_def(), model_version_dir,
                                 "model.graphdef", as_text=False)

def create_tf_modelconfig(
        create_savedmodel, models_dir, model_version, max_batch, dtype, shape):

    if not tu.validate_for_tf_model(dtype, dtype, dtype, shape, shape, shape):
        return

    # Use a different model name for the non-batching variant
    if create_savedmodel:
        model_name = tu.get_sequence_model_name(
            "savedmodel_nobatch" if max_batch == 0 else "savedmodel", dtype)
    else:
        model_name = tu.get_sequence_model_name(
            "graphdef_nobatch" if max_batch == 0 else "graphdef", dtype)

    config_dir = models_dir + "/" + model_name
    config = '''
name: "{}"
platform: "{}"
max_batch_size: {}
sequence_batching {{
  max_sequence_idle_microseconds: 5000000
  control_input [
    {{
      name: "START"
      control [
        {{
          kind: CONTROL_SEQUENCE_START
          {}_false_true: [ 0, 1 ]
        }}
      ]
    }},
    {{
      name: "READY"
      control [
        {{
          kind: CONTROL_SEQUENCE_READY
          {}_false_true: [ 0, 1 ]
        }}
      ]
    }}
  ]
}}
input [
  {{
    name: "INPUT"
    data_type: {}
    dims: [ {} ]
  }}
]
output [
  {{
    name: "OUTPUT"
    data_type: {}
    dims: [ 1 ]
  }}
]
instance_group [
  {{
    kind: KIND_GPU
  }}
]
'''.format(model_name,
           "tensorflow_savedmodel" if create_savedmodel else "tensorflow_graphdef",
           max_batch,
           "fp32" if dtype == np.float32 else "int32",
           "fp32" if dtype == np.float32 else "int32",
           np_to_model_dtype(dtype), tu.shape_to_dims_str(shape),
           np_to_model_dtype(dtype))

    try:
        os.makedirs(config_dir)
    except OSError as ex:
        pass # ignore existing dir

    with open(config_dir + "/config.pbtxt", "w") as cfile:
        cfile.write(config)


def create_netdef_modelfile(
        models_dir, model_version, max_batch, dtype, shape):

    if not tu.validate_for_c2_model(dtype, dtype, dtype, shape, shape, shape):
        return

    c2_dtype = np_to_c2_dtype(dtype)
    model_name = tu.get_sequence_model_name(
        "netdef_nobatch" if max_batch == 0 else "netdef", dtype)

    # Create the model. For now don't implement a proper accumulator
    # just return 0 if not-ready and 'INPUT'+'START' otherwise...  the
    # tests know to expect this.
    model = c2model_helper.ModelHelper(name=model_name)
    model.net.Add(["INPUT", "START"], "add")
    model.net.Sub(["READY", "READY"], "zeros")
    model.net.NE(["READY", "zeros"], "compare")
    model.net.Where(["compare", "add", "zeros"], "OUTPUT")

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
        models_dir, model_version, max_batch, dtype, shape):

    if not tu.validate_for_c2_model(dtype, dtype, dtype, shape, shape, shape):
        return

    model_name = tu.get_sequence_model_name(
        "netdef_nobatch" if max_batch == 0 else "netdef", dtype)
    config_dir = models_dir + "/" + model_name
    config = '''
name: "{}"
platform: "caffe2_netdef"
max_batch_size: {}
sequence_batching {{
  max_sequence_idle_microseconds: 5000000
  control_input [
    {{
      name: "START"
      control [
        {{
          kind: CONTROL_SEQUENCE_START
          {}_false_true: [ 0, 1 ]
        }}
      ]
    }},
    {{
      name: "READY"
      control [
        {{
          kind: CONTROL_SEQUENCE_READY
          {}_false_true: [ 0, 1 ]
        }}
      ]
    }}
  ]
}}
input [
  {{
    name: "INPUT"
    data_type: {}
    dims: [ {} ]
  }}
]
output [
  {{
    name: "OUTPUT"
    data_type: {}
    dims: [ 1 ]
  }}
]
instance_group [
  {{
    kind: KIND_CPU
  }}
]
'''.format(model_name, max_batch,
           "int32" if dtype == np.int32 else "fp32",
           "int32" if dtype == np.int32 else "fp32",
           np_to_model_dtype(dtype), tu.shape_to_dims_str(shape),
           np_to_model_dtype(dtype))

    try:
        os.makedirs(config_dir)
    except OSError as ex:
        pass # ignore existing dir

    with open(config_dir + "/config.pbtxt", "w") as cfile:
        cfile.write(config)


def create_plan_modelfile(
        models_dir, model_version, max_batch, dtype, shape):

    if not tu.validate_for_trt_model(dtype, dtype, dtype, shape, shape, shape):
        return

    trt_dtype = np_to_trt_dtype(dtype)

    # Create the model. For now don't implement a proper accumulator
    # just return 0 if not-ready and 'INPUT'+'START' otherwise...  the
    # tests know to expect this.
    G_LOGGER = trt.infer.ConsoleLogger(trt.infer.LogSeverity.INFO)
    builder = trt.infer.create_infer_builder(G_LOGGER)
    network = builder.create_network()
    in0 = network.add_input("INPUT", trt_dtype, shape)
    start0 = network.add_input("START", trt_dtype, [1, 1, 1])
    ready0 = network.add_input("READY", trt_dtype, [1, 1, 1])
    add = network.add_elementwise(in0, start0, trt.infer.ElementWiseOperation.SUM)
    out0 = network.add_elementwise(add.get_output(0), ready0, trt.infer.ElementWiseOperation.PROD)

    out0.get_output(0).set_name("OUTPUT")
    network.mark_output(out0.get_output(0))

    builder.set_max_batch_size(max(1, max_batch))
    builder.set_max_workspace_size(1 << 20)
    engine = builder.build_cuda_engine(network)
    network.destroy()

    model_name = tu.get_sequence_model_name(
        "plan_nobatch" if max_batch == 0 else "plan", dtype)
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
        models_dir, model_version, max_batch, dtype, shape):

    if not tu.validate_for_trt_model(dtype, dtype, dtype, shape, shape, shape):
        return

    model_name = tu.get_sequence_model_name(
        "plan_nobatch" if max_batch == 0 else "plan", dtype)
    config_dir = models_dir + "/" + model_name
    config = '''
name: "{}"
platform: "tensorrt_plan"
max_batch_size: {}
sequence_batching {{
  max_sequence_idle_microseconds: 5000000
  control_input [
    {{
      name: "START"
      control [
        {{
          kind: CONTROL_SEQUENCE_START
          {}_false_true: [ 0, 1 ]
        }}
      ]
    }},
    {{
      name: "READY"
      control [
        {{
          kind: CONTROL_SEQUENCE_READY
          {}_false_true: [ 0, 1 ]
        }}
      ]
    }}
  ]
}}
input [
  {{
    name: "INPUT"
    data_type: {}
    dims: [ {} ]
  }}
]
output [
  {{
    name: "OUTPUT"
    data_type: {}
    dims: [ 1, 1, 1 ]
  }}
]
instance_group [
  {{
    kind: KIND_GPU
  }}
]
'''.format(model_name, max_batch,
           "int32" if dtype == np.int32 else "fp32",
           "int32" if dtype == np.int32 else "fp32",
           np_to_model_dtype(dtype), tu.shape_to_dims_str(shape),
           np_to_model_dtype(dtype))

    try:
        os.makedirs(config_dir)
    except OSError as ex:
        pass # ignore existing dir

    with open(config_dir + "/config.pbtxt", "w") as cfile:
        cfile.write(config)

def create_onnx_modelfile(
        models_dir, model_version, max_batch, dtype, shape):

    if not tu.validate_for_onnx_model(dtype, dtype, dtype, shape, shape, shape):
        return

    model_name = tu.get_sequence_model_name(
        "onnx_nobatch" if max_batch == 0 else "onnx", dtype)
    model_version_dir = models_dir + "/" + model_name + "/" + str(model_version)

    # Create the model. For now don't implement a proper accumulator
    # just return 0 if not-ready and 'INPUT'+'START' otherwise...  the
    # tests know to expect this.
    onnx_dtype = np_to_onnx_dtype(dtype)
    onnx_input_shape, idx = tu.shape_to_onnx_shape(shape, 0)
    onnx_output_shape, idx = tu.shape_to_onnx_shape(shape, idx)

    # If the input is a string then use int32 for operation and just
    # cast to/from string for input and output.
    onnx_control_dtype = onnx_dtype
    if onnx_dtype == onnx.TensorProto.STRING:
        onnx_control_dtype = onnx.TensorProto.INT32

    batch_dim = [] if max_batch == 0 else [max_batch]

    onnx_input = onnx.helper.make_tensor_value_info("INPUT", onnx_dtype, batch_dim + onnx_input_shape)
    onnx_start = onnx.helper.make_tensor_value_info("START", onnx_control_dtype, batch_dim + [1])
    onnx_ready = onnx.helper.make_tensor_value_info("READY", onnx_control_dtype, batch_dim + [1])
    onnx_output = onnx.helper.make_tensor_value_info("OUTPUT", onnx_dtype, batch_dim + onnx_output_shape)

    internal_input = onnx.helper.make_node("Identity", ["INPUT"], ["_INPUT"])

    # cast int8, int16 input to higer precision int as Onnx Add/Sub operator doesn't support those type
    # Also casting String data type to int32
    if ((onnx_dtype == onnx.TensorProto.INT8) or (onnx_dtype == onnx.TensorProto.INT16) or
        (onnx_dtype == onnx.TensorProto.STRING)):
        internal_input = onnx.helper.make_node("Cast", ["INPUT"], ["_INPUT"], to=onnx.TensorProto.INT32)

    add = onnx.helper.make_node("Add", ["_INPUT", "START"], ["add"])
    # Take advantage of knowledge that the READY false value is 0 and true is 1
    mul = onnx.helper.make_node("Mul", ["READY", "add"], ["CAST"])
    cast = onnx.helper.make_node("Cast", ["CAST"], ["OUTPUT"], to=onnx_dtype)

    # Avoid cast from float16 to float16
    # (bug in Onnx Runtime, cast from float16 to float16 will become cast from float16 to float32)
    if onnx_dtype == onnx.TensorProto.FLOAT16:
        cast = onnx.helper.make_node("Identity", ["CAST"], ["OUTPUT"])

    onnx_nodes = [internal_input, add, mul, cast]
    onnx_inputs = [onnx_input, onnx_start, onnx_ready]
    onnx_outputs = [onnx_output]

    graph_proto = onnx.helper.make_graph(onnx_nodes, model_name, onnx_inputs, onnx_outputs)
    model_def = onnx.helper.make_model(graph_proto, producer_name="TRTIS")

    try:
        os.makedirs(model_version_dir)
    except OSError as ex:
        pass # ignore existing dir

    onnx.save(model_def, model_version_dir + "/model.onnx")


def create_onnx_modelconfig(
        models_dir, model_version, max_batch, dtype, shape):

    if not tu.validate_for_onnx_model(dtype, dtype, dtype, shape, shape, shape):
        return

    model_name = tu.get_sequence_model_name(
        "onnx_nobatch" if max_batch == 0 else "onnx", dtype)
    config_dir = models_dir + "/" + model_name

    instance_group_string = '''
instance_group [
  {
    kind: KIND_GPU
  }
]
'''

    # [TODO] move create_general_modelconfig() out of emu as it is general
    # enough for all backends to use
    config = emu.create_general_modelconfig(model_name, "onnxruntime_onnx", max_batch,
            [dtype], [shape], [None], [dtype], [shape], [None], [None],
            force_tensor_number_suffix=False, instance_group_str=instance_group_string)

    config += '''
sequence_batching {{
  max_sequence_idle_microseconds: 5000000
  control_input [
    {{
      name: "START"
      control [
        {{
          kind: CONTROL_SEQUENCE_START
          {type}_false_true: [ 0, 1 ]
        }}
      ]
    }},
    {{
      name: "READY"
      control [
        {{
          kind: CONTROL_SEQUENCE_READY
          {type}_false_true: [ 0, 1 ]
        }}
      ]
    }}
  ]
}}
'''.format(type="fp32" if dtype == np.float32 else "int32")

    try:
        os.makedirs(config_dir)
    except OSError as ex:
        pass # ignore existing dir

    with open(config_dir + "/config.pbtxt", "w") as cfile:
        cfile.write(config)


def create_models(models_dir, dtype, shape, no_batch=True):
    model_version = 1

    if FLAGS.graphdef:
        create_tf_modelconfig(False, models_dir, model_version, 8, dtype, shape)
        create_tf_modelfile(False, models_dir, model_version, 8, dtype, shape)
        if no_batch:
            create_tf_modelconfig(False, models_dir, model_version, 0, dtype, shape)
            create_tf_modelfile(False, models_dir, model_version, 0, dtype, shape)

    if FLAGS.savedmodel:
        create_tf_modelconfig(True, models_dir, model_version, 8, dtype, shape)
        create_tf_modelfile(True, models_dir, model_version, 8, dtype, shape)
        if no_batch:
            create_tf_modelconfig(True, models_dir, model_version, 0, dtype, shape)
            create_tf_modelfile(True, models_dir, model_version, 0, dtype, shape)

    if FLAGS.netdef:
        create_netdef_modelconfig(models_dir, model_version, 8, dtype, shape)
        create_netdef_modelfile(models_dir, model_version, 8, dtype, shape)
        if no_batch:
            create_netdef_modelconfig(models_dir, model_version, 0, dtype, shape)
            create_netdef_modelfile(models_dir, model_version, 0, dtype, shape)

    if FLAGS.tensorrt:
        create_plan_modelconfig(models_dir, model_version, 8, dtype, shape + [1, 1])
        create_plan_modelfile(models_dir, model_version, 8, dtype, shape + [1, 1])
        if no_batch:
            create_plan_modelconfig(models_dir, model_version, 0, dtype, shape + [1, 1])
            create_plan_modelfile(models_dir, model_version, 0, dtype, shape + [1, 1])

    if FLAGS.onnx:
        create_onnx_modelconfig(models_dir, model_version, 8, dtype, shape)
        create_onnx_modelfile(models_dir, model_version, 8, dtype, shape)
        if no_batch:
            create_onnx_modelconfig(models_dir, model_version, 0, dtype, shape)
            create_onnx_modelfile(models_dir, model_version, 0, dtype, shape)

    if FLAGS.ensemble:
        for pair in emu.platform_types_and_validation():
            if pair[0] == "plan":
                shape = shape + [1, 1]
            if not pair[1](dtype, dtype, dtype,
                            shape, shape, shape):
                continue

            emu.create_sequence_ensemble_modelconfig(
                pair[0], models_dir, 8, model_version, shape, dtype)
            emu.create_sequence_ensemble_modelfile(
                pair[0], models_dir, 8, model_version, shape, dtype)
            if no_batch:
                emu.create_sequence_ensemble_modelconfig(
                    pair[0], models_dir, 0, model_version, shape, dtype)
                emu.create_sequence_ensemble_modelfile(
                    pair[0], models_dir, 0, model_version, shape, dtype)


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
                        help='Generate Onnx models')
    parser.add_argument('--variable', required=False, action='store_true',
                        help='Used variable-shape tensors for input/output')
    parser.add_argument('--ensemble', required=False, action='store_true',
                        help='Generate ensemble models against the models'
                        + ' in all platforms. Note that the models generated'
                        + ' are not completed.')
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

    # Tests with models that accept fixed-shape input/output tensors
    if not FLAGS.variable:
        create_models(FLAGS.models_dir, np.float32, [1,])
        create_models(FLAGS.models_dir, np.int32, [1,])
        create_models(FLAGS.models_dir, np_dtype_string, [1,])

    # Tests with models that accept variable-shape input/output tensors
    if FLAGS.variable:
        create_models(FLAGS.models_dir, np.int32, [-1,], False)
        create_models(FLAGS.models_dir, np.float32, [-1,], False)
        create_models(FLAGS.models_dir, np_dtype_string, [-1,], False)

    if FLAGS.ensemble:
        # Create nop models used in ensemble
        for model_dtype in ["TYPE_INT32", "TYPE_FP32"]:
            # 3D shape for TensorRT Plan
            for model_shape in [(-1,), (-1, -1, -1)]:
                emu.create_nop_modelconfig(FLAGS.models_dir, model_shape, model_dtype)
