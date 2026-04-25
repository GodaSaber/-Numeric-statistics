#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdarg.h>

/* Print to both console and file */
void dual_print(FILE *fp, const char *fmt, ...);

/* Convert bytes to human-readable string */
void bytes_to_human(double bytes, char *out, int out_size);

#endif