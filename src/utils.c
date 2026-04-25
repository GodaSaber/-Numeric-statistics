#include "utils.h"
#include <stdio.h>
#include <stdarg.h>

void dual_print(FILE *fp, const char *fmt, ...)
{
    va_list args;

    /* Print to console */
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    /* Print to file */
    if (fp) {
        va_start(args, fmt);
        vfprintf(fp, fmt, args);
        va_end(args);
    }
}

void bytes_to_human(double bytes, char *out, int out_size)
{
    if (bytes >= 1024.0 * 1024.0 * 1024.0) {
        snprintf(out, out_size, "%.2f GB", bytes / (1024.0 * 1024.0 * 1024.0));
    } else if (bytes >= 1024.0 * 1024.0) {
        snprintf(out, out_size, "%.2f MB", bytes / (1024.0 * 1024.0));
    } else if (bytes >= 1024.0) {
        snprintf(out, out_size, "%.2f KB", bytes / 1024.0);
    } else {
        snprintf(out, out_size, "%.0f bytes", bytes);
    }
}