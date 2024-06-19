#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <boost/process.hpp>
#include <iostream>

#include "../includ/directed_tbs.h"
#define NumCores 2

int d2q_fifo;
int q2d_fifo;

double CPI[NumCores] = {};

namespace bp = boost::process;
char *workload_name = "./wokrload.gz";
char *mem_name = "./wokrload.dat";
char *gcpt_name = "./gcpt.bin";

std::string get_qemu_command(const char *gcpt, const char *workload, const char *ckpt_result_root, const char *cktp_config);
std::string get_pldm_command(const char *gcpt, const char *workload, uint64_t max_ins);
std::string get_bin2addr_command(const char *gpct, const char *workload);

int main(int argc, char *argv[]) {
    const char *detail_to_qemu_fifo_name = "./detail_to_qemu.fifo";
    const char *qemu_to_detail_fifo_name = "./qemu_to_detail.fifo";
    const char *emu_to_cpi_txt_name = "./emu_to_cpi_file.txt";
    const char *ckpt_list_name = "./ckpt_list.txt";
    const char *ckpt_result_root = "./ckpt_result_root";
    const char *ckpt_config = "run_with_emu";

    std::string qemu_command = get_qemu_command(gcpt_name, workload_name, ckpt_result_root, ckpt_config);
    std::string pldm_command = get_pldm_command(gcpt_name, workload_name, 40 * 1000000);
    std::string bin2addr_commmand = get_bin2addr_command(gcpt_name, workload_name);

    mkfifo(detail_to_qemu_fifo_name, 0666);
    mkfifo(qemu_to_detail_fifo_name, 0666);

    d2q_fifo = open(detail_to_qemu_fifo_name, O_WRONLY);
    q2d_fifo = open(qemu_to_detail_fifo_name, O_RDONLY);

    FILE *emu_result = fopen(emu_to_cpi_txt_name, O_RDONLY);
    FILE *workload_list = fopen(ckpt_list_name, O_RDONLY);
    char workload_name[32] = {};
    Detail2Qemu d2q_buf;
    Qemu2Detail q2d_buf;
//get workload
    fscanf(workload_list, "%s\n", workload_name);
//run qemu
    try {
        bp::child q(qemu_command);
        q.wait();
        q.exit_code();
    } catch (const std::exception &e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    //read qemu
    read(q2d_fifo, &q2d_buf, sizeof(Qemu2Detail));
    printf("Received from QEMU: %d %d %ld\n", q2d_buf.cpt_ready,
            q2d_buf.cpt_id, q2d_buf.total_inst_count);

//run bin2addr
    try {
        bp::child b(bin2addr_commmand);
        b.wait();
        b.exit_code();
    } catch (const std::exception &e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

//run emulate
    int coreid = 0;
    double cpi = 0;
    try {
        bp::child c(pldm_command);
        //cpi resut example [coreid,cpi]
        c.wait();
        c.exit_code();
    } catch (const std::exception &e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    for(int i = 0; i < NumCores; i++) {
        if (fscanf(emu_result, "%d,%lf\n", &coreid, &cpi) != 2) {
            printf("emu out result error\n");
            exit(0);
        } else if (coreid >= NumCores) {
            printf("coreid the maximum number of cores limit was exceeded\n");
        }
        d2q_buf.CPI[coreid] = cpi;
    }

    //update qemu
    printf("Sending to QEMU: %f %f\n", d2q_buf.CPI[0],
            d2q_buf.CPI[1]);
    write(d2q_fifo, &d2q_buf, sizeof(Detail2Qemu));

    return 0;
}

std::string get_qemu_command(const char *gcpt, const char *workload, const char *ckpt_result_root, const char *cktp_config) {
//example shell:
//$QEMU_HOME/build/qemu-system-riscv64 -bios $PAYLOAD -M nemu -nographic -m 8G -smp 2 -cpu rv64,v=true,vlen=128,h=false,sv39=true,sv48=false,sv57=false,sv64=false
// -workload $WROKLOAD_NAME -cpt-interval 200000000 -output-base-dir $CHECKPOINT_RESULT_ROOT -config-name $CHECKPOINT_CONFIG -checkpoint-mode UniformCheckpoint;
    std::string base_command = "../qemu/build/qemu-system-riscv64 ";
    std::string base_arggs = "-M nemu -nographic -m 8G -smp 2 -cpu rv64,v=true,vlen=128,h=false,sv39=true,sv48=false,sv57=false,sv64=false -checkpoint-mode UniformCheckpoint";
    char args[512];
    sprintf(args, "-bios %s -workload %s -cpt-interval 200000000 -output-base-dir %s -config-name %s" , gcpt, workload, ckpt_result_root, cktp_config);

    base_command.append(base_arggs);
    base_command.append(args);
    return base_command;
}

std::string get_pldm_command(const char *gcpt, const char *workload, uint64_t max_ins) {
    std::string base_command = "cd XiangShan && make pldm-run ";
    std::string base_arggs = "PLDM_EXTRA_ARGS=\"+=diff=./XiangShan/ready-to-run/riscv64-nemu-interpreter-dual-so ";
    char args[512];
    sprintf(args, "+wokrload=%s +gcpt-restore=%s +max-instrs=%ld \"" , workload, gcpt, max_ins);

    base_command.append(base_arggs);
    base_command.append(args);
    return base_command;
}

std::string get_bin2addr_command(const char *gcpt,const char *workload) {
//exmaple shell: ./bin2ddr -i $ckpt -o ./out.dat -m "row,ba,col,bg" -r $gcpt
    std::string base_command = "./Bin2Addr/bin2addr";
    std::string base_arggs = "-m \"row,ba,col,bg\" ";
    char args[512];
    sprintf(args, "-i %s -r %s " , workload, gcpt);

    base_command.append(base_arggs);
    base_command.append(args);
    return base_command;
}
