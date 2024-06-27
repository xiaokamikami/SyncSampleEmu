#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <boost/process.hpp>
#include <iostream>
#include <thread>

#include "../includ/directed_tbs.h"
#define NumCores 2

int d2q_fifo;
int q2d_fifo;

double CPI[NumCores] = {};

namespace bp = boost::process;

std::string get_qemu_command(const char *payload, const char *workload_name, const char *ckpt_result_root, const char *cktp_config, uint64_t sync_interval);
std::string get_pldm_command(const char *gcpt, const char *workload, uint64_t max_ins);
std::string get_bin2addr_command(const char *gpct, const char *workload);

int run_qemu(std::string command) {
    bp::ipstream pipe_stream;
    bp::child q(command, bp::std_out > bp::null);
    //bp::child q(command, bp::std_out > pipe_stream);
    // // 读取输出
    // std::string line;
    // while (pipe_stream && std::getline(pipe_stream, line) && !line.empty()) {
    //     std::cout << line << std::endl;
    // }
    q.wait();
    q.exit_code();
    return 1;
}

int main(int argc, char *argv[]) {
    const char *detail_to_qemu_fifo_name = "./detail_to_qemu.fifo";
    const char *qemu_to_detail_fifo_name = "./qemu_to_detail.fifo";
    const char *emu_to_cpi_txt_name = "./emu/emu_to_cpi_file.fifo";
    const char *gcpt_name = "./gcpt.bin";
    const char *workload_path = "./out.dat";
    char *payload_path = argv[1];
    char *workload_name = argv[2];
    char *ckpt_result_root = argv[3];
    char *ckpt_config = argv[4];
    char *ckpt_path = (char *)malloc(FILEPATH_BUF_SIZE);
    uint64_t sync_interval = 0;
    sscanf(argv[5], "%ld", &sync_interval);

    printf("Run sync emu with qemu\n");
    printf("payload=%s, workload_name=%s, result_root=%s , config=%s, sync_interval=%ld\n",
            payload_path, workload_name, ckpt_result_root, ckpt_config, sync_interval);
    std::string qemu_command = get_qemu_command(payload_path, workload_name, ckpt_result_root, ckpt_config, sync_interval);
    std::cout << "qemu command" << qemu_command << std::endl;
    mkfifo(detail_to_qemu_fifo_name, 0666);
    mkfifo(qemu_to_detail_fifo_name, 0666);

    FILE *emu_result = fopen(emu_to_cpi_txt_name, "r+");

    Detail2Qemu d2q_buf;
    Qemu2Detail q2d_buf;
    // run qemu
    std::thread t(run_qemu, qemu_command);
    printf("run qemu\n");
    sleep(5);
    printf("init ok\n");
    d2q_fifo = open(detail_to_qemu_fifo_name, O_WRONLY);
    q2d_fifo = open(qemu_to_detail_fifo_name, O_RDONLY);
    printf("into checkpoint sync while\n");
    while (1) {
        try {
            // read qemu
            printf("wait qemu fifo\n");
            ssize_t read_bytes = read(q2d_fifo, &q2d_buf, sizeof(Qemu2Detail));
            printf("get qemu fifo\n");
//#define SYNC_TEST
#ifdef SYNC_TEST
            while(1) {
                ssize_t read_bytes = read(q2d_fifo, &q2d_buf, sizeof(Qemu2Detail));
                printf("get qemu fifo\n");
                sleep(5);
                printf("update sync qemu\n");
                d2q_buf.CPI[0]=1;
                d2q_buf.CPI[1]=1;
                ssize_t write_bytes = write(d2q_fifo, &d2q_buf, sizeof(Detail2Qemu));
                if (write_bytes == -1) {
                    printf("write emu fifo faile\n");
                    return 1;
                }
            }
#endif
            if (read_bytes == -1) {
                printf("read qemu fifo faile\n");
                break;
            }
            printf("Received from QEMU: %d %d %ld\n", q2d_buf.cpt_ready,
                    q2d_buf.cpt_id, q2d_buf.total_inst_count);
            memcpy(ckpt_path, q2d_buf.checkpoint_path, FILEPATH_BUF_SIZE);

            // run bin2addr
            std::string bin2addr_commmand = get_bin2addr_command(gcpt_name, ckpt_path);
            bp::child b(bin2addr_commmand);
            b.wait();
            b.exit_code();

            //run emulate
            int coreid = 0;
            double cpi = 0;
            try {
                std::string pldm_command = get_pldm_command(gcpt_name, workload_path, 40 * 1000000);
                bp::child c(pldm_command);
                // cpi resut example [coreid,cpi]
                c.wait();
                c.exit_code();
            } catch (const std::exception &e) {
                std::cerr << "Exception: " << e.what() << std::endl;
                break;
            }
            // read cpi result
            for(int i = 0; i < NumCores; i++) {
                if (fscanf(emu_result, "%d,%lf\n", &coreid, &cpi) != 2) {
                    printf("emu out result error\n");
                    break;
                } else if (coreid >= NumCores) {
                    printf("coreid the maximum number of cores limit was exceeded\n");
                }
                d2q_buf.CPI[coreid] = cpi;
            }

            // update qemu
            printf("Sending to QEMU: %f %f\n", d2q_buf.CPI[0], d2q_buf.CPI[1]);
            ssize_t write_bytes = write(d2q_fifo, &d2q_buf, sizeof(Detail2Qemu));
            if (write_bytes == -1) {
                printf("write emu fifo faile\n");
                break;
            }
        } catch (const std::exception &e) {
            std::cerr << "Exception: " << e.what() << std::endl;
            break;
        }
    }

    return 0;
}

std::string get_qemu_command(const char *payload, const char *workload_name, const char *ckpt_result_root, const char *cktp_config, uint64_t sync_interval) {
    std::string base_command = "./qemu/build/qemu-system-riscv64 ";
    std::string base_arggs = "checkpoint-mode=SyncUniformCheckpoint -nographic -m 8G -smp 2 -cpu rv64,v=true,vlen=128";
    char args[512];
    sprintf(args, "-bios %s -M nemu,sync-interval=%ld,cpt-interval=200000000,output-base-dir=%s,config-name=%s,workload=%s,",
            payload, sync_interval, ckpt_result_root, cktp_config, workload_name);

    base_command.append(args);
    base_command.append(base_arggs);
    return base_command;
}

std::string get_pldm_command(const char *gcpt, const char *workload, uint64_t max_ins) {
    std::string base_command = "cd XiangShan && make pldm-run ";
    std::string base_arggs = "PLDM_EXTRA_ARGS=\"+=diff=./XiangShan/ready-to-run/riscv64-nemu-interpreter-dual-so ";
    char args[512];
    sprintf(args, "+wokrload=%s +gcpt-restore=%s +max-instrs=%ld \" && cd .." , workload, gcpt, max_ins);

    base_command.append(base_arggs);
    base_command.append(args);
    return base_command;
}

std::string get_bin2addr_command(const char *gcpt,const char *workload) {
//exmaple shell: ./bin2ddr -i $ckpt -o ./out.dat -m "row,ba,col,bg" -r $gcpt
    std::string base_command = "./Bin2Addr/bin2addr";
    std::string base_arggs = "-m \"row,ba,col,bg\" -o $(pwd)/out.dat";
    char args[512];
    sprintf(args, "-i %s -r %s " , workload, gcpt);

    base_command.append(base_arggs);
    base_command.append(args);
    return base_command;
}
