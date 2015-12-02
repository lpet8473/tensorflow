#!/usr/bin/env bash

set -e

export TF_NEED_CUDA=1
./configure
bazel build -c opt --config=cuda //tensorflow/...
