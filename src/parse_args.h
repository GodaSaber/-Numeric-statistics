#ifndef PARSE_ARGS_H
#define PARSE_ARGS_H


void print_usage(const char *prog);

unsigned long long parse_size(int argc, char **argv, int rank);

#endif