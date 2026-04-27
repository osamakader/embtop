#include "embtop.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

bool read_cpu_sample(struct cpu_sample *sample)
{
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) {
        return false;
    }

    char label[16];
    embtop_ull user = 0;
    embtop_ull nice = 0;
    embtop_ull system = 0;
    embtop_ull idle = 0;
    embtop_ull iowait = 0;
    embtop_ull irq = 0;
    embtop_ull softirq = 0;
    embtop_ull steal = 0;
    embtop_ull guest = 0;
    embtop_ull guest_nice = 0;

    int matched = fscanf(fp, "%15s %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
                         label,
                         &user,
                         &nice,
                         &system,
                         &idle,
                         &iowait,
                         &irq,
                         &softirq,
                         &steal,
                         &guest,
                         &guest_nice);
    fclose(fp);

    if (matched < 5 || strcmp(label, "cpu") != 0) {
        return false;
    }

    sample->idle = idle + iowait;
    sample->total = user + nice + system + idle + iowait + irq + softirq + steal + guest + guest_nice;

    const char *cur_paths[] = {
        "/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq",
        "/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_cur_freq",
    };
    const char *min_paths[] = {
        "/sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq",
        "/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_min_freq",
    };

    sample->cur_freq_khz = read_first_existing_long(cur_paths, sizeof(cur_paths) / sizeof(cur_paths[0]));
    sample->min_freq_khz = read_first_existing_long(min_paths, sizeof(min_paths) / sizeof(min_paths[0]));
    sample->temp_millic = -1;

    return true;
}

bool read_mem_sample(struct mem_sample *sample)
{
    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp) {
        return false;
    }

    char key[64];
    long value = 0;
    char unit[32];
    long mem_free = -1;
    long buffers = 0;
    long cached = 0;

    sample->total_kb = -1;
    sample->available_kb = -1;

    while (fscanf(fp, "%63s %ld %31s", key, &value, unit) == 3) {
        if (strcmp(key, "MemTotal:") == 0) {
            sample->total_kb = value;
        } else if (strcmp(key, "MemAvailable:") == 0) {
            sample->available_kb = value;
        } else if (strcmp(key, "MemFree:") == 0) {
            mem_free = value;
        } else if (strcmp(key, "Buffers:") == 0) {
            buffers = value;
        } else if (strcmp(key, "Cached:") == 0) {
            cached = value;
        }
    }

    fclose(fp);

    if (sample->available_kb < 0 && mem_free >= 0) {
        sample->available_kb = mem_free + buffers + cached;
    }

    return sample->total_kb > 0 && sample->available_kb >= 0;
}

long read_temperature_millic(void)
{
    DIR *dir = opendir("/sys/class/thermal");
    if (!dir) {
        return -1;
    }

    long temp = -1;
    struct dirent *entry = NULL;
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "thermal_zone", strlen("thermal_zone")) != 0) {
            continue;
        }

        char path[PATH_MAX];
        snprintf(path, sizeof(path), "/sys/class/thermal/%s/temp", entry->d_name);
        if (read_long_file(path, &temp)) {
            break;
        }
    }

    closedir(dir);
    return temp;
}
