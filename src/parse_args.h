#ifndef PARSE_ARGS_H
#define PARSE_ARGS_H

/* Print usage/help message */
void print_usage(const char *prog);

/* Parse command-line arguments and return total number of elements */
unsigned long long parse_size(int argc, char **argv, int rank);

#endif