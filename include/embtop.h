#ifndef EMBTOP_H
#define EMBTOP_H

#include <stdbool.h>
#include <stddef.h>

#define DEFAULT_DELAY_MS 1000
#define DEFAULT_TOP_N 5
#define MAX_NAME_LEN 64

typedef unsigned long long embtop_ull;

struct cpu_sample {
    embtop_ull total;
    embtop_ull idle;
    long cur_freq_khz;
    long min_freq_khz;
    long temp_millic;
};

struct mem_sample {
    long total_kb;
    long available_kb;
};

struct proc_sample {
    int pid;
    embtop_ull ticks;
    long rss_kb;
    double cpu_pct;
    char name[MAX_NAME_LEN];
};

struct proc_list {
    struct proc_sample *items;
    size_t len;
    size_t cap;
};

bool read_first_line(const char *path, char *buf, size_t size);
bool read_long_file(const char *path, long *value);
long read_first_existing_long(const char *const *paths, size_t count);

bool read_cpu_sample(struct cpu_sample *sample);
bool read_mem_sample(struct mem_sample *sample);
long read_temperature_millic(void);

void proc_list_free(struct proc_list *list);
bool read_processes(struct proc_list *list);
void calculate_process_cpu(struct proc_list *current,
                           const struct proc_list *prev,
                           embtop_ull total_delta);
void sort_processes_by_cpu(struct proc_list *list);

void render(const struct cpu_sample *cpu,
            const struct cpu_sample *prev_cpu,
            const struct mem_sample *mem,
            const struct proc_list *procs,
            int top_n,
            bool once);

#endif
