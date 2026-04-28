#include "parse_args.h"
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>

void print_usage(const char *prog)
{
    printf("\nUsage: mpirun -np <processes> %s <size>\n\n", prog);
    printf("  <size> can be:\n");
    printf("    --bytes <N>    : exact number of bytes (e.g. --bytes 10737418240)\n");
    printf("    --gb <N>       : size in gigabytes     (e.g. --gb 10)\n");
    printf("    --mb <N>       : size in megabytes     (e.g. --mb 512)\n");
    printf("    --elements <N> : exact element count   (e.g. --elements 2684354560)\n");
    printf("\n  Examples:\n");
    printf("    mpirun -np 4 %s --gb 10\n", prog);
    printf("    mpirun -np 4 %s --mb 500\n", prog);
    printf("    mpirun -np 4 %s --elements 100000000\n", prog);
    printf("    mpirun -np 4 %s --bytes 10737418240\n", prog);
    printf("\n  Default (no args): 10 GB\n\n");
}

unsigned long long parse_size(int argc, char **argv, int rank)
{
    unsigned long long total_elements = 0;

    if (argc < 3) {
        total_elements = DEFAULT_ELEMENTS;
        if (rank == 0)
            printf("No size specified. Using default: 10 GB\n\n");
        return total_elements;
    }

    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        if (rank == 0) print_usage(argv[0]);
        MPI_Finalize();
        exit(0);
    }

    if (strcmp(argv[1], "--gb") == 0) {
        double gb = atof(argv[2]);
        unsigned long long bytes = (unsigned long long)(gb * 1024.0 * 1024.0 * 1024.0);
        total_elements = bytes / sizeof(unsigned int);
    }
    else if (strcmp(argv[1],"--mb") == 0) {
        double mb = atof(argv[2]);
        unsigned long long bytes = (unsigned long long)(mb * 1024.0 * 1024.0);
        total_elements = bytes / sizeof(unsigned int);
    }
    else if (strcmp(argv[1], "--bytes") == 0) {
        unsigned long long bytes = strtoull(argv[2], NULL, 10);
        total_elements = bytes / sizeof(unsigned int);
    }
    else if (strcmp(argv[1], "--elements") == 0) {
        total_elements = strtoull(argv[2], NULL, 10);
    }
    else {
        if (rank == 0) {
            printf("ERROR: Unknown option '%s'\n", argv[1]);
            print_usage(argv[0]);
        }
        MPI_Finalize();
        exit(1);
    }

    if (total_elements == 0) {
        if (rank == 0) printf("ERROR: Size must be greater than 0\n");
        MPI_Finalize();
        exit(1);
    }

    return total_elements;
}