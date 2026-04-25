#include "frequency.h"
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>

void reduce_frequency(unsigned long long *local_freq,
                      unsigned long long *global_freq,
                      int rank)
{
    long long remaining = FREQ_SIZE;
    long long offset    = 0;
    unsigned long long *tmp = NULL;
    long long batch;

    while (remaining > 0) {
        batch = (remaining < FREQ_BATCH) ? remaining : FREQ_BATCH;

        if (rank == 0) {
            tmp = (unsigned long long *)malloc(batch * sizeof(unsigned long long));
            if (!tmp) {
                fprintf(stderr, "OOM in reduce_frequency\n");
                MPI_Abort(MPI_COMM_WORLD, 1);
            }
        }

        MPI_Reduce(local_freq + offset,
                    (rank == 0) ? tmp : NULL,
                    (int)batch,
                    MPI_UNSIGNED_LONG_LONG,
                    MPI_SUM,
                    0,
                    MPI_COMM_WORLD);

        if (rank == 0) {
            memcpy(global_freq + offset, tmp, batch * sizeof(unsigned long long));
            free(tmp);
        }

        offset    += batch;
        remaining -= batch;
    }
}