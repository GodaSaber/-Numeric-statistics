#ifndef FREQUENCY_H
#define FREQUENCY_H

/* 
 * Reduce local frequency arrays across all processes
 * into a global frequency array on root (rank 0).
 * Uses batched MPI_Reduce to save memory.
 */
void reduce_frequency(unsigned long long *local_freq,
                      unsigned long long *global_freq,
                      int rank);

#endif