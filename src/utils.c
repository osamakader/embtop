#include "embtop.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool read_first_line(const char *path, char *buf, size_t size)
{
    FILE *fp = fopen(path, "r");
    if (!fp) {
        return false;
    }

    bool ok = fgets(buf, (int)size, fp) != NULL;
    fclose(fp);

    if (ok) {
        buf[strcspn(buf, "\n")] = '\0';
    }
    return ok;
}

bool read_long_file(const char *path, long *value)
{
    char buf[64];
    char *end = NULL;

    if (!read_first_line(path, buf, sizeof(buf))) {
        return false;
    }

    errno = 0;
    long parsed = strtol(buf, &end, 10);
    if (errno != 0 || end == buf) {
        return false;
    }

    *value = parsed;
    return true;
}

long read_first_existing_long(const char *const *paths, size_t count)
{
    long value = -1;
    for (size_t i = 0; i < count; i++) {
        if (read_long_file(paths[i], &value)) {
            return value;
        }
    }
    return -1;
}
