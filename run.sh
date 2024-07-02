#!bin/bash
# 配置环境变量
export QEMU_HOME=$(pwd)/qemu # QEMU路径
export WORKLOAD_NAME=workload_name # 工作负载名称
export CHECKPOINT_RESULT_ROOT=$(pwd)/ 
# 最终生成的检查点的路径格式：/CHECKPOINT_RESULT_ROOT/CHECKPOINT_CONFIG/WORKLOAD_NAME/指令数/检查点.zstd/gz
export CHECKPOINT_CONFIG=dualcore_with_emu
export SYNC_INTERVAL=1000000000 # 触发同步QEMU与EMU的时钟数 默认1Billion

CHECKPOINT_PATH="/nfs/home/zstd-test"
mkdir result
CHECKPOINT_RESULT_PATH="./result/checkpoint_cpi.csv"

if [ -e "$CHECKPOINT_RESULT_PATH" ]; then
    rm "$CHECKPOINT_RESULT_PATH"
fi

for file in $CHECKPOINT_PATH/*.bin
do
    if [ -e "$file" ]; then
        var=$file
        work_load_name=${var#$CHECKPOINT_PATH/}
        work_load_name=${work_load_name%/*}
        export PAYLOAD=$file # 工作负载二进制文件路径
        #./syncmapemu argv[1] [2] [3] [4] [5] [6]
        ./syncmapemu $PAYLOAD $work_load_name $CHECKPOINT_RESULT_ROOT $CHECKPOINT_CONFIG $SYNC_INTERVAL $CHECKPOINT_RESULT_PATH
    fi
done
