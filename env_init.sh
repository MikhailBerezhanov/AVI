#!/bin/bash

source ../sim_open_sdk/sim_crosscompile/sim-crosscompile-env-init

CWD=$(cd $(dirname ${BASH_SOURCE[0]}) && pwd)

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${CWD}/sdk_addons/host/lib
export PROTOC=${CWD}/sdk_addons/host/bin/protoc
sudo ldconfig