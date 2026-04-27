#define _POSIX_C_SOURCE 200809L

#include "embtop.h"

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t keep_running = 1;

struct terminal_state {
    struct termios original;
    bool active;
};

static void handle_signal(int signum)
{
    (void)signum;
    keep_running = 0;
}

static bool terminal_setup(struct terminal_state *state)
{
    if (!isatty(STDIN_FILENO) || tcgetattr(STDIN_FILENO, &state->original) == -1) {
        state->active = false;
        return false;
    }

    struct termios raw = state->original;
    raw.c_lflag &= (tcflag_t) ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) == -1) {
        state->active = false;
        return false;
    }

    state->active = true;
    return true;
}

static void terminal_restore(const struct terminal_state *state)
{
    if (state->active) {
        tcsetattr(STDIN_FILENO, TCSANOW, &state->original);
    }
}

static void wait_for_refresh_or_quit(int delay_ms, bool read_input)
{
    if (!read_input) {
        struct timespec req;
        req.tv_sec = delay_ms / 1000;
        req.tv_nsec = (long)(delay_ms % 1000) * 1000000L;

        while (keep_running && nanosleep(&req, &req) == -1 && errno == EINTR) {
        }
        return;
    }

    struct timeval timeout;
    timeout.tv_sec = delay_ms / 1000;
    timeout.tv_usec = (delay_ms % 1000) * 1000;

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);

    int ready = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout);
    if (ready <= 0 || !FD_ISSET(STDIN_FILENO, &readfds)) {
        return;
    }

    char ch = '\0';
    while (read(STDIN_FILENO, &ch, 1) == 1) {
        if (ch == 'q' || ch == 'Q') {
            keep_running = 0;
            break;
        }
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
    struct terminal_state terminal = {0};

    if (!read_cpu_sample(&prev_cpu) || !read_processes(&prev_procs)) {
        fprintf(stderr, "failed to read initial /proc samples\n");
        proc_list_free(&prev_procs);
        return 1;
    }

    if (!once) {
        terminal_setup(&terminal);
        printf("\033[?25l");
        fflush(stdout);
    }

    do {
        wait_for_refresh_or_quit(delay_ms, terminal.active);
        if (!keep_running) {
            break;
        }

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
        terminal_restore(&terminal);
    }

    proc_list_free(&prev_procs);
    proc_list_free(&procs);
    return 0;
}
