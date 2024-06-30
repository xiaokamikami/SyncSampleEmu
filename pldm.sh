#!/bin/bash
checkpoint_path=${1:-default1}
echo get_workload_path $checkpoint_path
gcpt_path=$(pwd)/gcpt.bin
nemu=$(pwd)/riscv64-nemu-interpreter-dual-so
cd ./XiangShan
source ~/TOOLS/env.sh
make pldm-run PLDM_EXTRA_ARGS="+diff=$nemu +workload=$checkpoint_path +gcpt-restore=$gcpt_path +max-instrs=4000000 +overwrite_nbytes=64107252"
cd ..
