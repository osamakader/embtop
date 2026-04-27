#include "embtop.h"

#include <stdio.h>

static void format_bytes_from_kb(long kb, char *buf, size_t size)
{
    if (kb < 1024) {
        snprintf(buf, size, "%ldKB", kb);
    } else if (kb < 1024 * 1024) {
        snprintf(buf, size, "%ldMB", (kb + 512) / 1024);
    } else {
        snprintf(buf, size, "%.1fGB", (double)kb / (1024.0 * 1024.0));
    }
}

static void format_freq(long khz, char *buf, size_t size)
{
    if (khz < 0) {
        snprintf(buf, size, "n/a");
    } else if (khz >= 1000000) {
        snprintf(buf, size, "%.1fGHz", (double)khz / 1000000.0);
    } else {
        snprintf(buf, size, "%ldMHz", (khz + 500) / 1000);
    }
}

void render(const struct cpu_sample *cpu,
            const struct cpu_sample *prev_cpu,
            const struct mem_sample *mem,
            const struct proc_list *procs,
            int top_n,
            bool once)
{
    embtop_ull total_delta = cpu->total - prev_cpu->total;
    embtop_ull idle_delta = cpu->idle - prev_cpu->idle;
    double cpu_pct = 0.0;

    if (total_delta > 0 && total_delta >= idle_delta) {
        cpu_pct = ((double)(total_delta - idle_delta) * 100.0) / (double)total_delta;
    }

    long used_kb = mem->total_kb - mem->available_kb;
    char used_buf[32];
    char total_buf[32];
    char cur_freq[32];
    char min_freq[32];

    format_bytes_from_kb(used_kb, used_buf, sizeof(used_buf));
    format_bytes_from_kb(mem->total_kb, total_buf, sizeof(total_buf));
    format_freq(cpu->cur_freq_khz, cur_freq, sizeof(cur_freq));
    format_freq(cpu->min_freq_khz, min_freq, sizeof(min_freq));

    if (!once) {
        printf("\033[H\033[2J");
    }

    printf("embtop - minimal embedded monitor\n\n");
    printf("CPU:  %3.0f%%", cpu_pct);
    if (cpu->cur_freq_khz >= 0 || cpu->min_freq_khz >= 0) {
        printf("   (%s -> %s scaling)", cur_freq, min_freq);
    }
    printf("\n");

    if (cpu->temp_millic >= 0) {
        double temp_c = cpu->temp_millic > 1000 ? (double)cpu->temp_millic / 1000.0 : (double)cpu->temp_millic;
        printf("TEMP: %.0fC\n", temp_c);
    } else {
        printf("TEMP: n/a\n");
    }

    printf("MEM:  %s / %s\n\n", used_buf, total_buf);
    printf("Top processes:\n");
    printf("%-6s %5s %7s  %s\n", "PID", "CPU", "MEM", "NAME");

    int shown = 0;
    for (size_t i = 0; i < procs->len && shown < top_n; i++) {
        if (procs->items[i].cpu_pct <= 0.0 && shown > 0) {
            break;
        }

        char mem_buf[32];
        format_bytes_from_kb(procs->items[i].rss_kb, mem_buf, sizeof(mem_buf));
        printf("%-6d %4.0f%% %7s  %s\n",
               procs->items[i].pid,
               procs->items[i].cpu_pct,
               mem_buf,
               procs->items[i].name);
        shown++;
    }

    fflush(stdout);
}
