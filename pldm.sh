#!/bin/bash
checkpoint_path=${1:-default1}
echo get_workload_path $checkpoint_path
cd ./XiangShan
source ~/T00LS/env.sh
make pldm-run PLDM EXTRA ARGS="+=diff=./ready-to-run/riscv64-nemu-interpreter-dual-so +wokrload=$checkpoint_path +gcpt-restore=./gcpt.bin +max-instrs=4000000"
cd ..