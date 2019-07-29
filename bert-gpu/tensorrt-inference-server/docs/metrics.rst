..
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

.. _section-metrics:

Metrics
=======

The TensorRT Inference server provides `Prometheus
<https://prometheus.io/>`_ metrics indicating GPU and request
statistics. By default, these metrics are available at
http://localhost:8002/metrics. The metrics are only available by
accessing the endpoint, and are not pushed or published to any remote
server.

The inference server -\\-allow-metrics=false option can be used to
disable metric reporting and the -\\-metrics-port option can be used
to select a different port.

The following table describes the available metrics.

+--------------+----------------+---------------------------------------+-----------+-----------+
|Category      |Metric          |Description                            |Granularity|Frequency  |
|              |                |                                       |           |           |
+==============+================+=======================================+===========+===========+
|| GPU         |Power Usage     |GPU instantaneous power                |Per GPU    |Per second |
|| Utilization |                |                                       |           |           |
|              |                |                                       |           |           |
+              +----------------+---------------------------------------+-----------+-----------+
|              |Power Limit     |Maximum GPU power limit                |Per GPU    |Per second |
|              |                |                                       |           |           |
+              +----------------+---------------------------------------+-----------+-----------+
|              || Energy        || GPU energy consumption in joules     |Per GPU    |Per second |
|              || Consumption   || since the server started             |           |           |
+              +----------------+---------------------------------------+-----------+-----------+
|              |GPU Utilization || GPU utilization rate                 |Per GPU    |Per second |
|              |                || (0.0 - 1.0)                          |           |           |
+--------------+----------------+---------------------------------------+-----------+-----------+
|| GPU         || GPU Total     || Total GPU memory, in bytes           |Per GPU    |Per second |
|| Memory      || Memory        |                                       |           |           |
+              +----------------+---------------------------------------+-----------+-----------+
|              || GPU Used      || Used GPU memory, in bytes            |Per GPU    |Per second |
|              || Memory        |                                       |           |           |
+--------------+----------------+---------------------------------------+-----------+-----------+
|Count         |Request Count   || Number of inference requests         |Per model  |Per request|
|              |                |                                       |           |           |
|              |                |                                       |           |           |
|              |                |                                       |           |           |
+              +----------------+---------------------------------------+-----------+-----------+
|              |Execution Count || Number of inference executions       |Per model  |Per request|
|              |                || (request count / execution count     |           |           |
|              |                || = average dynamic batch size)        |           |           |
|              |                |                                       |           |           |
+              +----------------+---------------------------------------+-----------+-----------+
|              |Inference Count || Number of inferences performed       |Per model  |Per request|
|              |                || (one request counts as               |           |           |
|              |                || "batch size" inferences)             |           |           |
|              |                |                                       |           |           |
+--------------+----------------+---------------------------------------+-----------+-----------+
|Latency       |Request Time    || End-to-end inference request         |Per model  |Per request|
|              |                || handling time                        |           |           |
|              |                |                                       |           |           |
|              |                |                                       |           |           |
+              +----------------+---------------------------------------+-----------+-----------+
|              |Compute Time    || Time a request spends executing      |Per model  |Per request|
|              |                || the inference model (in the          |           |           |
|              |                || framework backend)                   |           |           |
|              |                |                                       |           |           |
+              +----------------+---------------------------------------+-----------+-----------+
|              |Queue Time      || Time a request spends waiting        |Per model  |Per request|
|              |                || in the queue                         |           |           |
|              |                |                                       |           |           |
|              |                |                                       |           |           |
|              |                |                                       |           |           |
+--------------+----------------+---------------------------------------+-----------+-----------+
