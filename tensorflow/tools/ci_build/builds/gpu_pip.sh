#!/usr/bin/env bash

set -e

export TF_NEED_CUDA=1
./configure
bazel build -c opt --config=cuda --color=yes //tensorflow/tools/pip_package:build_pip_package
rm -rf /root/.cache/tensorflow-pip
bazel-bin/tensorflow/tools/pip_package/build_pip_package /root/.cache/tensorflow-pip
