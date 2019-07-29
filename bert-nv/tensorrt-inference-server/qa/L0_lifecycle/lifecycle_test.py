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

import sys
sys.path.append("../common")

from builtins import range
from future.utils import iteritems
import os
import shutil
import time
import unittest
import numpy as np
import infer_util as iu
import test_util as tu
from tensorrtserver.api import *
import tensorrtserver.api.server_status_pb2 as server_status

class LifeCycleTest(unittest.TestCase):

    def test_parse_error_noexit(self):
        # Server was started with invalid args and
        # --exit-on-error=false so expect it to be running with
        # SERVER_FAILED_TO_INITIALIZE status.
        # --strict-readiness=false so server is not live and not ready
        try:
            for pair in [("localhost:8000", ProtocolType.HTTP), ("localhost:8001", ProtocolType.GRPC)]:
                ctx = ServerStatusContext(pair[0], pair[1], None, True)
                ss = ctx.get_server_status()
                self.assertEqual(os.environ["TENSORRT_SERVER_VERSION"], ss.version)
                self.assertEqual("inference:0", ss.id)
                self.assertEqual(server_status.SERVER_FAILED_TO_INITIALIZE, ss.ready_state)
                self.assertEqual(len(ss.model_status), 0)
                uptime = ss.uptime_ns
                self.assertGreater(uptime, 0)

                hctx = ServerHealthContext(pair[0], pair[1], True)
                self.assertFalse(hctx.is_ready())
                self.assertFalse(hctx.is_live())

        except InferenceServerException as ex:
            self.assertTrue(False, "unexpected error {}".format(ex))

    def test_parse_error_noexit_strict(self):
        # Server was started with invalid args and
        # --exit-on-error=false so expect it to be running with
        # SERVER_FAILED_TO_INITIALIZE status.
        # --strict-readiness=false so server is not live and not ready
        try:
            for pair in [("localhost:8000", ProtocolType.HTTP), ("localhost:8001", ProtocolType.GRPC)]:
                ctx = ServerStatusContext(pair[0], pair[1], None, True)
                ss = ctx.get_server_status()
                self.assertEqual(os.environ["TENSORRT_SERVER_VERSION"], ss.version)
                self.assertEqual("inference:0", ss.id)
                self.assertEqual(server_status.SERVER_FAILED_TO_INITIALIZE, ss.ready_state)
                self.assertEqual(len(ss.model_status), 0)
                uptime = ss.uptime_ns
                self.assertGreater(uptime, 0)

                hctx = ServerHealthContext(pair[0], pair[1], True)
                self.assertFalse(hctx.is_ready())
                self.assertFalse(hctx.is_live())

        except InferenceServerException as ex:
            self.assertTrue(False, "unexpected error {}".format(ex))

    def test_parse_error_modelfail(self):
        # --strict-readiness=true so server is live but not ready
        input_size = 16
        tensor_shape = (input_size,)

        # Server was started but with a model that fails to load
        try:
            for pair in [("localhost:8000", ProtocolType.HTTP), ("localhost:8001", ProtocolType.GRPC)]:
                model_name = tu.get_model_name('graphdef', np.float32, np.float32, np.float32)
                ctx = ServerStatusContext(pair[0], pair[1], model_name, True)
                ss = ctx.get_server_status()
                self.assertEqual(os.environ["TENSORRT_SERVER_VERSION"], ss.version)
                self.assertEqual("inference:0", ss.id)
                self.assertEqual(server_status.SERVER_READY, ss.ready_state)
                uptime = ss.uptime_ns
                self.assertGreater(uptime, 0)

                self.assertEqual(len(ss.model_status), 1)
                self.assertTrue(model_name in ss.model_status,
                                "expected status for model " + model_name)
                for (k, v) in iteritems(ss.model_status[model_name].version_status):
                    self.assertEqual(v.ready_state, server_status.MODEL_UNAVAILABLE)

                hctx = ServerHealthContext(pair[0], pair[1], True)
                self.assertFalse(hctx.is_ready())
                self.assertTrue(hctx.is_live())

        except InferenceServerException as ex:
            self.assertTrue(False, "unexpected error {}".format(ex))

        try:
            iu.infer_exact(self, 'graphdef', tensor_shape, 1,
                           np.float32, np.float32, np.float32)
            self.assertTrue(False, "expected error for unavailable model " + model_name)
        except InferenceServerException as ex:
            self.assertEqual("inference:0", ex.server_id())
            self.assertTrue(
                ex.message().startswith(
                    "Inference request for unknown model 'graphdef_float32_float32_float32'"))

    def test_dynamic_model_load_unload(self):
        input_size = 16
        tensor_shape = (input_size,)
        savedmodel_name = tu.get_model_name('savedmodel', np.float32, np.float32, np.float32)
        netdef_name = tu.get_model_name('netdef', np.float32, np.float32, np.float32)

        # Make sure savedmodel model is not in the status (because
        # initially it is not in the model store)
        try:
            for pair in [("localhost:8000", ProtocolType.HTTP), ("localhost:8001", ProtocolType.GRPC)]:
                ctx = ServerStatusContext(pair[0], pair[1], savedmodel_name, True)
                ss = ctx.get_server_status()
                self.assertTrue(False, "expected status failure for " + savedmodel_name)
        except InferenceServerException as ex:
            self.assertEqual("inference:0", ex.server_id())
            self.assertTrue(
                ex.message().startswith("no status available for unknown model"))

        # Add savedmodel model to the model store and give it time to
        # load. Make sure that it has a status and is ready.
        try:
            shutil.copytree(savedmodel_name, "models/" + savedmodel_name)
            time.sleep(5) # wait for model to load
            for pair in [("localhost:8000", ProtocolType.HTTP), ("localhost:8001", ProtocolType.GRPC)]:
                ctx = ServerStatusContext(pair[0], pair[1], savedmodel_name, True)
                ss = ctx.get_server_status()
                self.assertEqual(os.environ["TENSORRT_SERVER_VERSION"], ss.version)
                self.assertEqual("inference:0", ss.id)
                self.assertEqual(server_status.SERVER_READY, ss.ready_state)

                self.assertEqual(len(ss.model_status), 1)
                self.assertTrue(savedmodel_name in ss.model_status,
                                "expected status for model " + savedmodel_name)
                for (k, v) in iteritems(ss.model_status[savedmodel_name].version_status):
                    self.assertEqual(v.ready_state, server_status.MODEL_READY)
        except InferenceServerException as ex:
            self.assertTrue(False, "unexpected error {}".format(ex))

        # Run inference on the just loaded model
        try:
            iu.infer_exact(self, 'savedmodel', tensor_shape, 1,
                           np.float32, np.float32, np.float32, swap=True)
        except InferenceServerException as ex:
            self.assertTrue(False, "unexpected error {}".format(ex))

        # Make sure savedmodel has execution stats in the status.
        expected_exec_cnt = 0
        try:
            for pair in [("localhost:8000", ProtocolType.HTTP), ("localhost:8001", ProtocolType.GRPC)]:
                ctx = ServerStatusContext(pair[0], pair[1], savedmodel_name, True)
                ss = ctx.get_server_status()
                self.assertEqual(os.environ["TENSORRT_SERVER_VERSION"], ss.version)
                self.assertEqual("inference:0", ss.id)
                self.assertEqual(server_status.SERVER_READY, ss.ready_state)

                self.assertEqual(len(ss.model_status), 1)
                self.assertTrue(savedmodel_name in ss.model_status,
                                "expected status for model " + savedmodel_name)
                self.assertTrue(3 in ss.model_status[savedmodel_name].version_status,
                                "expected status for version 3 of model " + savedmodel_name)

                version_status = ss.model_status[savedmodel_name].version_status[3]
                self.assertEqual(version_status.ready_state, server_status.MODEL_READY)
                self.assertGreater(version_status.model_execution_count, 0)
                expected_exec_cnt = version_status.model_execution_count
        except InferenceServerException as ex:
            self.assertTrue(False, "unexpected error {}".format(ex))

        # Remove savedmodel model from the model store and give it
        # time to unload. Make sure that it has a status but is
        # unavailable.
        try:
            shutil.rmtree("models/" + savedmodel_name)
            time.sleep(5) # wait for model to unload
            for pair in [("localhost:8000", ProtocolType.HTTP), ("localhost:8001", ProtocolType.GRPC)]:
                ctx = ServerStatusContext(pair[0], pair[1], savedmodel_name, True)
                ss = ctx.get_server_status()
                self.assertEqual(os.environ["TENSORRT_SERVER_VERSION"], ss.version)
                self.assertEqual("inference:0", ss.id)
                self.assertEqual(server_status.SERVER_READY, ss.ready_state)

                self.assertEqual(len(ss.model_status), 1)
                self.assertTrue(savedmodel_name in ss.model_status,
                                "expected status for model " + savedmodel_name)
                self.assertTrue(3 in ss.model_status[savedmodel_name].version_status,
                                "expected status for version 3 of model " + savedmodel_name)

                version_status = ss.model_status[savedmodel_name].version_status[3]
                self.assertEqual(version_status.ready_state, server_status.MODEL_UNAVAILABLE)
                self.assertEqual(version_status.model_execution_count, expected_exec_cnt)
        except InferenceServerException as ex:
            self.assertTrue(False, "unexpected error {}".format(ex))

        # Model is removed so inference should fail
        try:
            iu.infer_exact(self, 'savedmodel', tensor_shape, 1,
                           np.float32, np.float32, np.float32, swap=True)
            self.assertTrue(False, "expected error for unavailable model " + savedmodel_name)
        except InferenceServerException as ex:
            self.assertEqual("inference:0", ex.server_id())
            self.assertTrue(
                ex.message().startswith(
                    "Inference request for unknown model 'savedmodel_float32_float32_float32'"))

        # Add back the same model. The status/stats should be reset.
        try:
            shutil.copytree(savedmodel_name, "models/" + savedmodel_name)
            time.sleep(5) # wait for model to load
            for pair in [("localhost:8000", ProtocolType.HTTP), ("localhost:8001", ProtocolType.GRPC)]:
                ctx = ServerStatusContext(pair[0], pair[1], savedmodel_name, True)
                ss = ctx.get_server_status()
                self.assertEqual(os.environ["TENSORRT_SERVER_VERSION"], ss.version)
                self.assertEqual("inference:0", ss.id)
                self.assertEqual(server_status.SERVER_READY, ss.ready_state)

                self.assertEqual(len(ss.model_status), 1)
                self.assertTrue(savedmodel_name in ss.model_status,
                                "expected status for model " + savedmodel_name)
                for (k, v) in iteritems(ss.model_status[savedmodel_name].version_status):
                    self.assertEqual(v.ready_state, server_status.MODEL_READY)
                    self.assertEqual(v.model_execution_count, 0)
        except InferenceServerException as ex:
            self.assertTrue(False, "unexpected error {}".format(ex))

        # Remove original model from the model store and give it time
        # to unload. Make sure that it has a status but is
        # unavailable.
        try:
            shutil.rmtree("models/" + netdef_name)
            time.sleep(5) # wait for model to unload
            for pair in [("localhost:8000", ProtocolType.HTTP), ("localhost:8001", ProtocolType.GRPC)]:
                ctx = ServerStatusContext(pair[0], pair[1], netdef_name, True)
                ss = ctx.get_server_status()
                self.assertEqual(os.environ["TENSORRT_SERVER_VERSION"], ss.version)
                self.assertEqual("inference:0", ss.id)
                self.assertEqual(server_status.SERVER_READY, ss.ready_state)

                self.assertEqual(len(ss.model_status), 1)
                self.assertTrue(netdef_name in ss.model_status,
                                "expected status for model " + netdef_name)
                self.assertTrue(3 in ss.model_status[netdef_name].version_status,
                                "expected status for version 3 of model " + netdef_name)

                version_status = ss.model_status[netdef_name].version_status[3]
                self.assertEqual(version_status.ready_state, server_status.MODEL_UNAVAILABLE)
        except InferenceServerException as ex:
            self.assertTrue(False, "unexpected error {}".format(ex))

        # Model is removed so inference should fail
        try:
            iu.infer_exact(self, 'netdef', tensor_shape, 1,
                           np.float32, np.float32, np.float32, swap=True)
            self.assertTrue(False, "expected error for unavailable model " + netdef_name)
        except InferenceServerException as ex:
            self.assertEqual("inference:0", ex.server_id())
            self.assertTrue(
                ex.message().startswith(
                    "Inference request for unknown model 'netdef_float32_float32_float32'"))

    def test_dynamic_model_load_unload_disabled(self):
        input_size = 16
        tensor_shape = (input_size,)
        savedmodel_name = tu.get_model_name('savedmodel', np.float32, np.float32, np.float32)
        netdef_name = tu.get_model_name('netdef', np.float32, np.float32, np.float32)

        # Make sure savedmodel model is not in the status (because
        # initially it is not in the model store)
        try:
            for pair in [("localhost:8000", ProtocolType.HTTP), ("localhost:8001", ProtocolType.GRPC)]:
                ctx = ServerStatusContext(pair[0], pair[1], savedmodel_name, True)
                ss = ctx.get_server_status()
                self.assertTrue(False, "expected status failure for " + savedmodel_name)
        except InferenceServerException as ex:
            self.assertEqual("inference:0", ex.server_id())
            self.assertGreater(ex.request_id(), 0)
            self.assertTrue(
                ex.message().startswith("no status available for unknown model"))

        # Add savedmodel model to the model store and give it time to
        # load. But it shouldn't load because dynamic loading is disabled.
        try:
            shutil.copytree(savedmodel_name, "models/" + savedmodel_name)
            time.sleep(5) # wait for model to load
            for pair in [("localhost:8000", ProtocolType.HTTP), ("localhost:8001", ProtocolType.GRPC)]:
                ctx = ServerStatusContext(pair[0], pair[1], savedmodel_name, True)
                ss = ctx.get_server_status()
                self.assertTrue(False, "expected status failure for " + savedmodel_name)
        except InferenceServerException as ex:
            self.assertEqual("inference:0", ex.server_id())
            self.assertGreater(ex.request_id(), 0)
            self.assertTrue(
                ex.message().startswith("no status available for unknown model"))

        # Run inference which should fail because the model isn't there
        try:
            iu.infer_exact(self, 'savedmodel', tensor_shape, 1,
                           np.float32, np.float32, np.float32, swap=True)
            self.assertTrue(False, "expected error for unavailable model " + savedmodel_name)
        except InferenceServerException as ex:
            self.assertEqual("inference:0", ex.server_id())
            self.assertGreater(ex.request_id(), 0)
            self.assertTrue(
                ex.message().startswith("no status available for unknown model"))

        # Remove one of the original models from the model
        # store. Unloading is disabled so it should remain available
        # in the status.
        try:
            shutil.rmtree("models/" + netdef_name)
            time.sleep(5) # wait for model to unload (but it shouldn't)
            for pair in [("localhost:8000", ProtocolType.HTTP), ("localhost:8001", ProtocolType.GRPC)]:
                ctx = ServerStatusContext(pair[0], pair[1], netdef_name, True)
                ss = ctx.get_server_status()
                self.assertEqual(os.environ["TENSORRT_SERVER_VERSION"], ss.version)
                self.assertEqual("inference:0", ss.id)
                self.assertEqual(server_status.SERVER_READY, ss.ready_state)

                self.assertEqual(len(ss.model_status), 1)
                self.assertTrue(netdef_name in ss.model_status,
                                "expected status for model " + netdef_name)
                self.assertTrue(3 in ss.model_status[netdef_name].version_status,
                                "expected status for version 3 of model " + netdef_name)

                version_status = ss.model_status[netdef_name].version_status[3]
                self.assertEqual(version_status.ready_state, server_status.MODEL_READY)

        except InferenceServerException as ex:
            self.assertTrue(False, "unexpected error {}".format(ex))

        # Run inference to make sure model still being served even
        # though deleted from model store
        try:
            iu.infer_exact(self, 'netdef', tensor_shape, 1,
                           np.float32, np.float32, np.float32, swap=True)
        except InferenceServerException as ex:
            self.assertTrue(False, "unexpected error {}".format(ex))

    def test_dynamic_version_load_unload(self):
        input_size = 16
        tensor_shape = (input_size,)
        graphdef_name = tu.get_model_name('graphdef', np.int32, np.int32, np.int32)

        # There are 3 versions. Make sure that all have status and are
        # ready.
        try:
            for pair in [("localhost:8000", ProtocolType.HTTP), ("localhost:8001", ProtocolType.GRPC)]:
                ctx = ServerStatusContext(pair[0], pair[1], graphdef_name, True)
                ss = ctx.get_server_status()
                self.assertEqual(os.environ["TENSORRT_SERVER_VERSION"], ss.version)
                self.assertEqual("inference:0", ss.id)
                self.assertEqual(server_status.SERVER_READY, ss.ready_state)

                self.assertEqual(len(ss.model_status), 1)
                self.assertTrue(graphdef_name in ss.model_status,
                                "expected status for model " + graphdef_name)
                self.assertEqual(len(ss.model_status[graphdef_name].version_status), 3)
                for (k, v) in iteritems(ss.model_status[graphdef_name].version_status):
                    self.assertEqual(v.ready_state, server_status.MODEL_READY)
        except InferenceServerException as ex:
            self.assertTrue(False, "unexpected error {}".format(ex))

        # Run inference on version 1 to make sure it is available
        try:
            iu.infer_exact(self, 'graphdef', tensor_shape, 1,
                           np.int32, np.int32, np.int32, swap=False,
                           model_version=1)
        except InferenceServerException as ex:
            self.assertTrue(False, "unexpected error {}".format(ex))

        # Make sure version 1 has execution stats in the status.
        expected_exec_cnt = 0
        try:
            for pair in [("localhost:8000", ProtocolType.HTTP), ("localhost:8001", ProtocolType.GRPC)]:
                ctx = ServerStatusContext(pair[0], pair[1], graphdef_name, True)
                ss = ctx.get_server_status()
                self.assertEqual(os.environ["TENSORRT_SERVER_VERSION"], ss.version)
                self.assertEqual("inference:0", ss.id)
                self.assertEqual(server_status.SERVER_READY, ss.ready_state)

                self.assertEqual(len(ss.model_status), 1)
                self.assertTrue(graphdef_name in ss.model_status,
                                "expected status for model " + graphdef_name)
                self.assertTrue(1 in ss.model_status[graphdef_name].version_status,
                                "expected status for version 1 of model " + graphdef_name)

                version_status = ss.model_status[graphdef_name].version_status[1]
                self.assertEqual(version_status.ready_state, server_status.MODEL_READY)
                self.assertGreater(version_status.model_execution_count, 0)
                expected_exec_cnt = version_status.model_execution_count
        except InferenceServerException as ex:
            self.assertTrue(False, "unexpected error {}".format(ex))

        # Remove version 1 from the model store and give it time to
        # unload. Make sure that it has a status but is unavailable.
        try:
            shutil.rmtree("models/" + graphdef_name + "/1")
            time.sleep(5) # wait for version to unload
            for pair in [("localhost:8000", ProtocolType.HTTP), ("localhost:8001", ProtocolType.GRPC)]:
                ctx = ServerStatusContext(pair[0], pair[1], graphdef_name, True)
                ss = ctx.get_server_status()
                self.assertEqual(os.environ["TENSORRT_SERVER_VERSION"], ss.version)
                self.assertEqual("inference:0", ss.id)
                self.assertEqual(server_status.SERVER_READY, ss.ready_state)

                self.assertEqual(len(ss.model_status), 1)
                self.assertTrue(graphdef_name in ss.model_status,
                                "expected status for model " + graphdef_name)
                self.assertTrue(1 in ss.model_status[graphdef_name].version_status,
                                "expected status for version 1 of model " + graphdef_name)

                version_status = ss.model_status[graphdef_name].version_status[1]
                self.assertEqual(version_status.ready_state, server_status.MODEL_UNAVAILABLE)
                self.assertEqual(version_status.model_execution_count, expected_exec_cnt)
        except InferenceServerException as ex:
            self.assertTrue(False, "unexpected error {}".format(ex))

        # Version is removed so inference should fail
        try:
            iu.infer_exact(self, 'graphdef', tensor_shape, 1,
                           np.int32, np.int32, np.int32, swap=False,
                           model_version=1)
            self.assertTrue(False, "expected error for unavailable model " + graphdef_name)
        except InferenceServerException as ex:
            self.assertEqual("inference:0", ex.server_id())
            self.assertTrue(
                ex.message().startswith(
                    "Inference request for unknown model 'graphdef_int32_int32_int32'"))

        # Add back the same version. The status/stats should be
        # retained for versions (note that this is different behavior
        # than if a model is removed and then added back).
        try:
            shutil.copytree("models/" + graphdef_name + "/2",
                            "models/" + graphdef_name + "/1")
            time.sleep(5) # wait for model to load
            for pair in [("localhost:8000", ProtocolType.HTTP), ("localhost:8001", ProtocolType.GRPC)]:
                ctx = ServerStatusContext(pair[0], pair[1], graphdef_name, True)
                ss = ctx.get_server_status()
                self.assertEqual(os.environ["TENSORRT_SERVER_VERSION"], ss.version)
                self.assertEqual("inference:0", ss.id)
                self.assertEqual(server_status.SERVER_READY, ss.ready_state)

                self.assertEqual(len(ss.model_status), 1)
                self.assertTrue(graphdef_name in ss.model_status,
                                "expected status for model " + graphdef_name)
                self.assertEqual(len(ss.model_status[graphdef_name].version_status), 3)
                for (k, v) in iteritems(ss.model_status[graphdef_name].version_status):
                    self.assertEqual(v.ready_state, server_status.MODEL_READY)
                    if k == 1:
                        self.assertEqual(v.model_execution_count, expected_exec_cnt)
                    else:
                        self.assertEqual(v.model_execution_count, 0)
        except InferenceServerException as ex:
            self.assertTrue(False, "unexpected error {}".format(ex))

        # Add another version from the model store.
        try:
            shutil.copytree("models/" + graphdef_name + "/2",
                            "models/" + graphdef_name + "/7")
            time.sleep(5) # wait for version to load
            for pair in [("localhost:8000", ProtocolType.HTTP), ("localhost:8001", ProtocolType.GRPC)]:
                ctx = ServerStatusContext(pair[0], pair[1], graphdef_name, True)
                ss = ctx.get_server_status()
                self.assertEqual(os.environ["TENSORRT_SERVER_VERSION"], ss.version)
                self.assertEqual("inference:0", ss.id)
                self.assertEqual(server_status.SERVER_READY, ss.ready_state)

                self.assertEqual(len(ss.model_status), 1)
                self.assertTrue(graphdef_name in ss.model_status,
                                "expected status for model " + graphdef_name)
                self.assertTrue(7 in ss.model_status[graphdef_name].version_status,
                                "expected status for version 7 of model " + graphdef_name)

                self.assertEqual(len(ss.model_status[graphdef_name].version_status), 4)
                for (k, v) in iteritems(ss.model_status[graphdef_name].version_status):
                    self.assertEqual(v.ready_state, server_status.MODEL_READY)
        except InferenceServerException as ex:
            self.assertTrue(False, "unexpected error {}".format(ex))

    def test_dynamic_version_load_unload_disabled(self):
        input_size = 16
        tensor_shape = (input_size,)
        graphdef_name = tu.get_model_name('graphdef', np.int32, np.int32, np.int32)

        # Add a new version to the model store and give it time to
        # load. But it shouldn't load because dynamic loading is
        # disabled.
        try:
            shutil.copytree("models/" + graphdef_name + "/2",
                            "models/" + graphdef_name + "/7")
            time.sleep(5) # wait for model to load
            for pair in [("localhost:8000", ProtocolType.HTTP), ("localhost:8001", ProtocolType.GRPC)]:
                ctx = ServerStatusContext(pair[0], pair[1], graphdef_name, True)
                ss = ctx.get_server_status()
                self.assertEqual(os.environ["TENSORRT_SERVER_VERSION"], ss.version)
                self.assertEqual("inference:0", ss.id)
                self.assertEqual(server_status.SERVER_READY, ss.ready_state)

                self.assertEqual(len(ss.model_status), 1)
                self.assertTrue(graphdef_name in ss.model_status,
                                "expected status for model " + graphdef_name)
                self.assertFalse(7 in ss.model_status[graphdef_name].version_status,
                                "unexpected status for version 7 of model " + graphdef_name)
                self.assertEqual(len(ss.model_status[graphdef_name].version_status), 3)
        except InferenceServerException as ex:
            self.assertTrue(False, "unexpected error {}".format(ex))

        # Remove one of the original versions from the model
        # store. Unloading is disabled so it should remain available
        # in the status.
        try:
            shutil.rmtree("models/" + graphdef_name + "/1")
            time.sleep(5) # wait for version to unload (but it shouldn't)
            for pair in [("localhost:8000", ProtocolType.HTTP), ("localhost:8001", ProtocolType.GRPC)]:
                ctx = ServerStatusContext(pair[0], pair[1], graphdef_name, True)
                ss = ctx.get_server_status()
                self.assertEqual(os.environ["TENSORRT_SERVER_VERSION"], ss.version)
                self.assertEqual("inference:0", ss.id)
                self.assertEqual(server_status.SERVER_READY, ss.ready_state)

                self.assertEqual(len(ss.model_status), 1)
                self.assertTrue(graphdef_name in ss.model_status,
                                "expected status for model " + graphdef_name)
                self.assertTrue(1 in ss.model_status[graphdef_name].version_status,
                                "expected status for version 1 of model " + graphdef_name)

                self.assertEqual(len(ss.model_status[graphdef_name].version_status), 3)
                for (k, v) in iteritems(ss.model_status[graphdef_name].version_status):
                    self.assertEqual(v.ready_state, server_status.MODEL_READY)
        except InferenceServerException as ex:
            self.assertTrue(False, "unexpected error {}".format(ex))

        # Run inference to make sure model still being served even
        # though version deleted from model store
        try:
            iu.infer_exact(self, 'graphdef', tensor_shape, 1,
                           np.int32, np.int32, np.int32, swap=False,
                           model_version=1)
        except InferenceServerException as ex:
            self.assertTrue(False, "unexpected error {}".format(ex))

    def test_dynamic_model_modify(self):
        input_size = 16
        models_base = ('savedmodel', 'plan')
        models_shape = ((input_size,), (input_size, 1, 1))
        models = list()
        for m in models_base:
            models.append(tu.get_model_name(m, np.float32, np.float32, np.float32))

        # Make sure savedmodel and plan are in the status
        for model_name in models:
            try:
                for pair in [("localhost:8000", ProtocolType.HTTP), ("localhost:8001", ProtocolType.GRPC)]:
                    ctx = ServerStatusContext(pair[0], pair[1], model_name, True)
                    ss = ctx.get_server_status()
                    self.assertEqual(os.environ["TENSORRT_SERVER_VERSION"], ss.version)
                    self.assertEqual("inference:0", ss.id)
                    self.assertEqual(server_status.SERVER_READY, ss.ready_state)

                    self.assertEqual(len(ss.model_status), 1)
                    self.assertTrue(model_name in ss.model_status,
                                    "expected status for model " + model_name)
                    for (k, v) in iteritems(ss.model_status[model_name].version_status):
                        self.assertEqual(v.ready_state, server_status.MODEL_READY)
            except InferenceServerException as ex:
                self.assertTrue(False, "unexpected error {}".format(ex))

        # Run inference on the model, both versions 1 and 3
        for version in (1, 3):
            for model_name, model_shape in zip(models_base, models_shape):
                try:
                    iu.infer_exact(self, model_name, model_shape, 1,
                                   np.float32, np.float32, np.float32, swap=(version == 3),
                                   model_version=version)
                except InferenceServerException as ex:
                    self.assertTrue(False, "unexpected error {}".format(ex))

        # Change the model configuration to use wrong label file
        for base_name, model_name in zip(models_base, models):
            shutil.copyfile("config.pbtxt.wrong." + base_name, "models/" + model_name + "/config.pbtxt")

        time.sleep(5) # wait for models to reload
        for model_name in models:
            for model_name, model_shape in zip(models_base, models_shape):
                try:
                    iu.infer_exact(self, model_name, model_shape, 1,
                                   np.float32, np.float32, np.float32, swap=(version == 3),
                                   model_version=version, output0_raw=False)
                    self.assertTrue(False, "expected error for wrong label for " + model_name)
                except AssertionError as ex:
                    self.assertTrue(str(ex).startswith("'label"), str(ex))

        # Change the model configuration to use correct label file and to have
        # the default version policy (so that only version 3) is available.
        for base_name, model_name in zip(models_base, models):
            shutil.copyfile("config.pbtxt." + base_name, "models/" + model_name + "/config.pbtxt")

        time.sleep(5) # wait for models to reload
        for model_name in models:
            try:
                for pair in [("localhost:8000", ProtocolType.HTTP), ("localhost:8001", ProtocolType.GRPC)]:
                    ctx = ServerStatusContext(pair[0], pair[1], model_name, True)
                    ss = ctx.get_server_status()
                    self.assertEqual(os.environ["TENSORRT_SERVER_VERSION"], ss.version)
                    self.assertEqual("inference:0", ss.id)
                    self.assertEqual(server_status.SERVER_READY, ss.ready_state)
                    self.assertEqual(len(ss.model_status), 1)
                    self.assertTrue(model_name in ss.model_status,
                                    "expected status for model " + model_name)
                    self.assertTrue(1 in ss.model_status[model_name].version_status,
                                    "expected status for version 1 of model " + model_name)
                    self.assertTrue(3 in ss.model_status[model_name].version_status,
                                    "expected status for version 3 of model " + model_name)
                    self.assertEqual(ss.model_status[model_name].version_status[1].ready_state,
                                     server_status.MODEL_UNAVAILABLE)
                    self.assertEqual(ss.model_status[model_name].version_status[3].ready_state,
                                     server_status.MODEL_READY)
            except InferenceServerException as ex:
                self.assertTrue(False, "unexpected error {}".format(ex))

        # Attempt inferencing using version 1, should fail since
        # change in model policy makes that no longer available.
        for model_name, model_shape in zip(models_base, models_shape):
            try:
                iu.infer_exact(self, model_name, model_shape, 1,
                               np.float32, np.float32, np.float32, swap=False,
                               model_version=1)
                self.assertTrue(False, "expected error for unavailable model " + model_name)
            except InferenceServerException as ex:
                self.assertEqual("inference:0", ex.server_id())
                self.assertTrue(
                    ex.message().startswith("Inference request for unknown model"))

        # Version 3 should continue to work...
        for model_name, model_shape in zip(models_base, models_shape):
            try:
                iu.infer_exact(self, model_name, model_shape, 1,
                               np.float32, np.float32, np.float32, swap=True,
                               model_version=3)
            except InferenceServerException as ex:
                self.assertTrue(False, "unexpected error {}".format(ex))

if __name__ == '__main__':
    unittest.main()
