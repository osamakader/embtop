#define _POSIX_C_SOURCE 200809L

#include "embtop.h"

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t keep_running = 1;

static void handle_signal(int signum)
{
    (void)signum;
    keep_running = 0;
}

static void sleep_ms(int delay_ms)
{
    struct timespec req;
    req.tv_sec = delay_ms / 1000;
    req.tv_nsec = (long)(delay_ms % 1000) * 1000000L;

    while (keep_running && nanosleep(&req, &req) == -1 && errno == EINTR) {
    }
}

static void usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s [-1] [-d delay_ms] [-n processes]\n"
            "\n"
            "  -1              sample once and exit\n"
            "  -d delay_ms     refresh interval in milliseconds (default: %d)\n"
            "  -n processes    number of processes to show (default: %d)\n",
            prog,
            DEFAULT_DELAY_MS,
            DEFAULT_TOP_N);
}

int main(int argc, char **argv)
{
    int delay_ms = DEFAULT_DELAY_MS;
    int top_n = DEFAULT_TOP_N;
    bool once = false;

    int opt = 0;
    while ((opt = getopt(argc, argv, "1d:n:h")) != -1) {
        switch (opt) {
        case '1':
            once = true;
            break;
        case 'd':
            delay_ms = atoi(optarg);
            if (delay_ms < 100) {
                fprintf(stderr, "delay must be at least 100ms\n");
                return 2;
            }
            break;
        case 'n':
            top_n = atoi(optarg);
            if (top_n < 1) {
                fprintf(stderr, "process count must be positive\n");
                return 2;
            }
            break;
        case 'h':
            usage(argv[0]);
            return 0;
        default:
            usage(argv[0]);
            return 2;
        }
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    struct cpu_sample prev_cpu = {0};
    struct cpu_sample cpu = {0};
    struct mem_sample mem = {0};
    struct proc_list prev_procs = {0};
    struct proc_list procs = {0};

    if (!read_cpu_sample(&prev_cpu) || !read_processes(&prev_procs)) {
        fprintf(stderr, "failed to read initial /proc samples\n");
        proc_list_free(&prev_procs);
        return 1;
    }

    if (!once) {
        printf("\033[?25l");
        fflush(stdout);
    }

    do {
        sleep_ms(delay_ms);

        if (!read_cpu_sample(&cpu) || !read_mem_sample(&mem) || !read_processes(&procs)) {
            fprintf(stderr, "failed to read system samples\n");
            break;
        }

        cpu.temp_millic = read_temperature_millic();
        embtop_ull total_delta = cpu.total - prev_cpu.total;
        calculate_process_cpu(&procs, &prev_procs, total_delta);
        sort_processes_by_cpu(&procs);

        render(&cpu, &prev_cpu, &mem, &procs, top_n, once);

        struct cpu_sample tmp_cpu = prev_cpu;
        prev_cpu = cpu;
        cpu = tmp_cpu;

        struct proc_list tmp_procs = prev_procs;
        prev_procs = procs;
        procs = tmp_procs;
    } while (keep_running && !once);

    if (!once) {
        printf("\033[?25h\n");
    }

    proc_list_free(&prev_procs);
    proc_list_free(&procs);
    return 0;
}
