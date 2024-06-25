#!bin/bash
# 配置环境变量
export QEMU_HOME=$(pwd)/qemu # QEMU路径
export WORKLOAD_NAME=workload_name # 工作负载名称
export CHECKPOINT_RESULT_ROOT=$(pwd)/checkpoint_root 
# 最终生成的检查点的路径格式：/CHECKPOINT_RESULT_ROOT/CHECKPOINT_CONFIG/WORKLOAD_NAME/指令数/检查点.zstd/gz
export CHECKPOINT_CONFIG=checkpoint_config_name

CHECKPOINT_PATH="/nfs/home/zstd-test"
CHECKPOINT_LIST="$CHECKPOINT_PATH/list-test.txt"

if [ ! -f "$CHECKPOINT_LIST" ]; then
    touch "$CHECKPOINT_LIST"
fi

for file in $CHECKPOINT_PATH/*/*/*.zstd
do
    if [ -e "$file" ]; then
        var=$file
        export PAYLOAD=$file # 工作负载二进制文件路径
        ./syncmapemu
    fi
done