#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <boost/process.h>
#include <iostream.h>
#include <cstring>
#include <vector>

#include "directed_tbs.h"
#define NumCores 2

int d2q_fifo;
int q2d_fifo;

double CPI[NumCores] = {};

namespace bp = boost::process;
char *workload_name = "./wokrload.gz";
char *mem_name = "./wokrload.dat";
char *gcpt_name = "./gcpt.bin";

std::string get_pldm_command(char *gpct, char *workload, uint64_t max_ins);
std::string get_bin2addr_command(char *gpct, char *workload);
int main(int argc, char *argv[])
{
    const char *detail_to_qemu_fifo_name = "./detail_to_qemu.fifo";
    const char *qemu_to_detail_fifo_name = "./qemu_to_detail.fifo";
    const char *emu_to_cpi_txt_name = "./emu_to_cpi_file.txt"

    std::string pldm_command = get_pldm_command(gcpt_name, workload_name, 40000000);
    std::string bin2addr_commmand = get_bin2addr_command(gcpt_name, workload_name);

    // creact pipline
    bp::ipstream pipe_stream;

    mkfifo(detail_to_qemu_fifo_name, 0666);
    mkfifo(qemu_to_detail_fifo_name, 0666);

    d2q_fifo = open(detail_to_qemu_fifo_name, O_WRONLY);
    q2d_fifo = open(qemu_to_detail_fifo_name, O_RDONLY);

    FILE *emu_result = fopen(emu_to_cpi_txt_name, O_RDONLY);
    Detail2Qemu d2q_buf;
    Qemu2Detail q2d_buf;

//run qemu
    read(q2d_fifo, &q2d_buf, sizeof(Qemu2Detail));
    printf("Received from QEMU: %d %d %ld\n", q2d_buf.cpt_ready,
            q2d_buf.cpt_id, q2d_buf.total_inst_count);

//run bin2addr

//run emulate
    bp::child c(pldm_command, bp::std_out > pipe_stream);
    //cpi resut example [coreid,cpi]
    int coreid = -1;
    double cpi = 0;

    for(int i = 0; i < NumCores; i++) {
        if (fsancf(emu_result, "%d,%lf\n", &coreid, &cpi) != 2) {
            printf("emu out result error\n");
            exit(0);
        }
        if (coreid > (NumCores -1)) {
            printf("coreid the maximum number of cores limit was exceeded\n");
        }
        d2q_buf.CPI[coreid] = cpi;
    }

    //update qemu
    printf("Sending to QEMU: %f %f\n", d2q_buf.CPI[i % 2],
            d2q_buf.CPI[(i + 1) % 2]);
    write(d2q_fifo, &d2q_buf, sizeof(Detail2Qemu));

    return 0;
}


std::string get_pldm_command(char *gpct, char *workload, uint64_t max_ins) {
    std::string base_command = "make pldm-run";
    std::string base_arggs = "PLDM_EXTRA_ARGS=""+=diff=./XiangShan/ready-to-run/riscv64-nemu-interpreter-dual-so";

    char args[512];
    sprintf(args, "+wokrload=%s +gcpt-restore=%s +max-instrs=%ld""" , workload, gcpt);

    std::strcat(base_command, base_arggs);
    base_command.append(args);

    return base_command;
}

std::string get_bin2addr_command(char *gpct, char *workload) {
//exmaple ./bin2ddr -i $ckpt -o ./out.dat -m "row,ba,col,bg" -r $gcpt



}
