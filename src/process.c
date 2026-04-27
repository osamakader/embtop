#define _POSIX_C_SOURCE 200809L

#include "embtop.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static bool proc_list_push(struct proc_list *list, const struct proc_sample *sample)
{
    if (list->len == list->cap) {
        size_t next_cap = list->cap == 0 ? 128 : list->cap * 2;
        struct proc_sample *next = realloc(list->items, next_cap * sizeof(*next));
        if (!next) {
            return false;
        }
        list->items = next;
        list->cap = next_cap;
    }

    list->items[list->len++] = *sample;
    return true;
}

static bool is_pid_dir(const char *name)
{
    if (name[0] == '\0') {
        return false;
    }

    for (const char *p = name; *p; p++) {
        if (!isdigit((unsigned char)*p)) {
            return false;
        }
    }
    return true;
}

static bool read_proc_name(int pid, char *name, size_t size)
{
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "/proc/%d/comm", pid);
    if (read_first_line(path, name, size)) {
        return true;
    }

    snprintf(name, size, "%d", pid);
    return false;
}

static bool read_proc_stat(int pid, embtop_ull *ticks, long *rss_kb)
{
    char path[PATH_MAX];
    char line[4096];

    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    if (!read_first_line(path, line, sizeof(line))) {
        return false;
    }

    char *close_paren = strrchr(line, ')');
    if (!close_paren || close_paren[1] != ' ') {
        return false;
    }

    char *saveptr = NULL;
    char *token = strtok_r(close_paren + 2, " ", &saveptr);
    int field = 3;
    embtop_ull utime = 0;
    embtop_ull stime = 0;
    long rss_pages = 0;

    while (token) {
        if (field == 14) {
            utime = strtoull(token, NULL, 10);
        } else if (field == 15) {
            stime = strtoull(token, NULL, 10);
        } else if (field == 24) {
            rss_pages = strtol(token, NULL, 10);
            break;
        }

        token = strtok_r(NULL, " ", &saveptr);
        field++;
    }

    if (field < 24) {
        return false;
    }

    long page_kb = sysconf(_SC_PAGESIZE) / 1024;
    *ticks = utime + stime;
    *rss_kb = rss_pages > 0 ? rss_pages * page_kb : 0;
    return true;
}

static const struct proc_sample *find_prev_proc(const struct proc_list *prev, int pid)
{
    for (size_t i = 0; i < prev->len; i++) {
        if (prev->items[i].pid == pid) {
            return &prev->items[i];
        }
    }
    return NULL;
}

static int compare_proc_cpu(const void *left, const void *right)
{
    const struct proc_sample *a = left;
    const struct proc_sample *b = right;

    if (b->cpu_pct > a->cpu_pct) {
        return 1;
    }
    if (b->cpu_pct < a->cpu_pct) {
        return -1;
    }
    return b->rss_kb - a->rss_kb;
}

void proc_list_free(struct proc_list *list)
{
    free(list->items);
    list->items = NULL;
    list->len = 0;
    list->cap = 0;
}

bool read_processes(struct proc_list *list)
{
    DIR *dir = opendir("/proc");
    if (!dir) {
        return false;
    }

    list->len = 0;
    struct dirent *entry = NULL;
    while ((entry = readdir(dir)) != NULL) {
        if (!is_pid_dir(entry->d_name)) {
            continue;
        }

        int pid = atoi(entry->d_name);
        struct proc_sample sample = {0};
        sample.pid = pid;

        if (!read_proc_stat(pid, &sample.ticks, &sample.rss_kb)) {
            continue;
        }
        read_proc_name(pid, sample.name, sizeof(sample.name));

        if (!proc_list_push(list, &sample)) {
            closedir(dir);
            return false;
        }
    }

    closedir(dir);
    return true;
}

void calculate_process_cpu(struct proc_list *current,
                           const struct proc_list *prev,
                           embtop_ull total_delta)
{
    if (total_delta == 0) {
        return;
    }

    for (size_t i = 0; i < current->len; i++) {
        const struct proc_sample *old = find_prev_proc(prev, current->items[i].pid);
        if (!old || current->items[i].ticks < old->ticks) {
            continue;
        }

        embtop_ull proc_delta = current->items[i].ticks - old->ticks;
        current->items[i].cpu_pct = (double)proc_delta * 100.0 / (double)total_delta;
    }
}

void sort_processes_by_cpu(struct proc_list *list)
{
    qsort(list->items, list->len, sizeof(list->items[0]), compare_proc_cpu);
}
