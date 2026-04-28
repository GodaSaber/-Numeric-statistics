#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <mpi.h>

#include "config.h"
#include "utils.h"
#include "parse_args.h"
#include "frequency.h"

int main(int argc, char **argv)
{
    int rank, size;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    /* ---- Parse size from command line ---- */
    unsigned long long TOTAL_ELEMENTS = parse_size(argc, argv, rank);

    /* ---- 1. Divide load ---- */
    long long base_count  = (long long)(TOTAL_ELEMENTS / size);
    long long extra       = (long long)(TOTAL_ELEMENTS % size);
    long long local_count = base_count + (rank < extra ? 1 : 0);

    /* ---- Value range: [-HALF_MAX, +HALF_MAX) ---- */
    int HALF_MAX = (int)(MAX_VAL / 2);
    int VAL_MIN  = -HALF_MAX;
    int VAL_MAX  =  HALF_MAX;         /* exclusive upper bound */
    int OFFSET   =  HALF_MAX;         /* shift so index 0 == VAL_MIN */

    /* FREQ_SIZE must be >= MAX_VAL to cover the full range.
       If config.h defines FREQ_SIZE == MAX_VAL, this just works. */

    /* ---- Size calculations ---- */
    double total_data_bytes   = (double)TOTAL_ELEMENTS * sizeof(int);
    double local_data_bytes   = (double)local_count * sizeof(int);
    double chunk_mem_mb       = (double)CHUNK_SIZE * sizeof(int) / (1024.0 * 1024.0);
    double freq_mem_mb        = (double)FREQ_SIZE * sizeof(unsigned long long) / (1024.0 * 1024.0);
    double mem_per_process_mb = chunk_mem_mb + freq_mem_mb;

    char total_size_str[64], local_size_str[64];
    bytes_to_human(total_data_bytes, total_size_str, sizeof(total_size_str));
    bytes_to_human(local_data_bytes, local_size_str, sizeof(local_size_str));

    if (rank == 0) {
        printf("=============================================\n");
        printf("      Distributed Numeric Statistics         \n");
        printf("=============================================\n\n");
        printf("--- Data Info ---\n");
        printf("Total elements       : %llu\n", (unsigned long long)TOTAL_ELEMENTS);
        printf("Element size         : %lu bytes (int)\n", sizeof(int));
        printf("Total data size      : %s (%.0f bytes)\n", total_size_str, total_data_bytes);
        printf("Value range          : [%d, %d)\n\n", VAL_MIN, VAL_MAX);
        printf("--- Distribution Info ---\n");
        printf("MPI processes        : %d\n", size);
        printf("Elements per process : %lld (~%s)\n", local_count, local_size_str);
        printf("Chunk size           : %lld elements (~%.2f MB)\n", (long long)CHUNK_SIZE, chunk_mem_mb);
        printf("Freq array per proc  : %u buckets (~%.2f MB)\n", (unsigned int)FREQ_SIZE, freq_mem_mb);
        printf("Memory per process   : ~%.2f MB (chunk + freq)\n\n", mem_per_process_mb);
    }

    /* ---- 2. Local statistics variables ---- */
    int            local_min = INT_MAX;
    int            local_max = INT_MIN;
    long long      local_sum = 0LL;      /* signed to handle negatives */

    unsigned long long *local_freq =
        (unsigned long long *)calloc(FREQ_SIZE, sizeof(unsigned long long));
    if (!local_freq) {
        fprintf(stderr, "[%d] Cannot allocate freq array\n", rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    long long buf_size = (local_count < CHUNK_SIZE) ? local_count : CHUNK_SIZE;
    int *buf = (int *)malloc(buf_size * sizeof(int));
    if (!buf) {
        fprintf(stderr, "[%d] Cannot allocate chunk buffer\n", rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    /* ---- 3. Process data in chunks ---- */
    double t_start = MPI_Wtime();

    long long remaining = local_count;
    unsigned int seed = (unsigned int)(rank * 9973 + time(NULL));
    srand(seed);

    double local_data_gb = local_data_bytes / (1024.0 * 1024.0 * 1024.0);

    while (remaining > 0) {
        long long this_chunk = (remaining < buf_size) ? remaining : buf_size;

        /* Generate random values in [VAL_MIN, VAL_MAX) */
        for (long long i = 0; i < this_chunk; i++) {
            buf[i] = (rand() % MAX_VAL) - HALF_MAX;   /* maps [0,MAX_VAL) → [-HALF_MAX, HALF_MAX) */
        }

        for (long long i = 0; i < this_chunk; i++) {
            int v = buf[i];
            if (v < local_min) local_min = v;
            if (v > local_max) local_max = v;
            local_sum += v;
            local_freq[v + OFFSET]++;   /* shift into [0, FREQ_SIZE) */
        }

        remaining -= this_chunk;

        if (rank == 0) {
            double done = (double)(local_count - remaining);
            double pct  = 100.0 * done / (double)local_count;
            double done_gb = done * sizeof(int) / (1024.0*1024.0*1024.0);
            printf("\r  [Rank 0] Progress: %.1f%% (%.2f / %.2f GB processed)",
                   pct, done_gb, local_data_gb);
            fflush(stdout);
        }
    }

    if (rank == 0) printf("\n\n");
    free(buf);

    /* ---- 4. Global reductions ---- */
    int            global_min;
    int            global_max;
    long long      global_sum;

    MPI_Reduce(&local_min, &global_min, 1, MPI_INT,       MPI_MIN, 0, MPI_COMM_WORLD);
    MPI_Reduce(&local_max, &global_max, 1, MPI_INT,       MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&local_sum, &global_sum, 1, MPI_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);

    unsigned long long *global_freq = NULL;
    if (rank == 0) {
        global_freq = (unsigned long long *)calloc(FREQ_SIZE, sizeof(unsigned long long));
        if (!global_freq) {
            fprintf(stderr, "Cannot allocate global freq\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
    }
    reduce_frequency(local_freq, global_freq, rank);
    free(local_freq);

    double t_end = MPI_Wtime();

    /* ---- 5. Print results to console AND save to file ---- */
    if (rank == 0) {
        double mean = (double)global_sum / (double)TOTAL_ELEMENTS;
        double elapsed = t_end - t_start;
        double total_data_gb = total_data_bytes / (1024.0 * 1024.0 * 1024.0);
        double throughput_gb = total_data_gb / elapsed;

        FILE *fp = fopen(OUTPUT_FILE, "w");
        if (!fp) {
            fprintf(stderr, "WARNING: Cannot open %s for writing.\n", OUTPUT_FILE);
        }

        dual_print(fp, "=============================================\n");
        dual_print(fp, "      Distributed Numeric Statistics         \n");
        dual_print(fp, "=============================================\n\n");

        dual_print(fp, "--- Data Info ---\n");
        dual_print(fp, "Total elements       : %llu\n", (unsigned long long)TOTAL_ELEMENTS);
        dual_print(fp, "Element size         : %lu bytes (int)\n", sizeof(int));
        dual_print(fp, "Total data size      : %s (%.0f bytes)\n", total_size_str, total_data_bytes);
        dual_print(fp, "Value range          : [%d, %d)\n\n", VAL_MIN, VAL_MAX);

        dual_print(fp, "--- Distribution Info ---\n");
        dual_print(fp, "MPI processes        : %d\n", size);
        dual_print(fp, "Elements per process : %lld (~%s)\n", local_count, local_size_str);
        dual_print(fp, "Chunk size           : %lld elements (~%.2f MB)\n", (long long)CHUNK_SIZE, chunk_mem_mb);
        dual_print(fp, "Freq array per proc  : %u buckets (~%.2f MB)\n", (unsigned int)FREQ_SIZE, freq_mem_mb);
        dual_print(fp, "Memory per process   : ~%.2f MB (chunk + freq)\n\n", mem_per_process_mb);

        dual_print(fp, "============== Results ==============\n\n");

        dual_print(fp, "--- Size Summary ---\n");
        dual_print(fp, "Total data processed : %s (%llu elements)\n",
                   total_size_str, (unsigned long long)TOTAL_ELEMENTS);
        dual_print(fp, "Data per process     : %s (%lld elements)\n",
                   local_size_str, local_count);
        dual_print(fp, "Memory used per proc : ~%.2f MB\n\n", mem_per_process_mb);

        dual_print(fp, "--- Statistics ---\n");
        dual_print(fp, "Min                  : %d\n",   global_min);
        dual_print(fp, "Max                  : %d\n",   global_max);
        dual_print(fp, "Sum                  : %lld\n", global_sum);
        dual_print(fp, "Mean                 : %.6f\n", mean);

        dual_print(fp, "\n--- Performance ---\n");
        dual_print(fp, "Total time           : %.4f seconds\n", elapsed);
        dual_print(fp, "Throughput           : %.2f GB/s\n", throughput_gb);
        dual_print(fp, "Processes used       : %d\n", size);

        /* ---- Top 10 ---- */
        dual_print(fp, "\n--- Top 10 Most Frequent Values ---\n");
        dual_print(fp, "   Value   |   Count\n");
        dual_print(fp, "-----------+-----------\n");

        int                top_val[10];
        unsigned long long top_cnt[10];
        memset(top_cnt, 0, sizeof(top_cnt));
        memset(top_val, 0, sizeof(top_val));

        for (unsigned int idx = 0; idx < FREQ_SIZE; idx++) {
            int mi = 0;
            for (int k = 1; k < 10; k++)
                if (top_cnt[k] < top_cnt[mi]) mi = k;
            if (global_freq[idx] > top_cnt[mi]) {
                top_cnt[mi] = global_freq[idx];
                top_val[mi] = (int)idx - OFFSET;   /* convert back to signed value */
            }
        }
        /* Sort descending */
        for (int i = 0; i < 9; i++)
            for (int j = i + 1; j < 10; j++)
                if (top_cnt[j] > top_cnt[i]) {
                    unsigned long long tc = top_cnt[i]; top_cnt[i] = top_cnt[j]; top_cnt[j] = tc;
                    int                tv = top_val[i]; top_val[i] = top_val[j]; top_val[j] = tv;
                }
        for (int i = 0; i < 10; i++)
            dual_print(fp, "  %8d | %9llu\n", top_val[i], top_cnt[i]);

        /* Full frequency to file only */
        if (fp) {
            fprintf(fp, "\n--- Full Frequency Array ---\n");
            fprintf(fp, "   Value   |   Count\n");
            fprintf(fp, "-----------+-----------\n");
            for (unsigned int idx = 0; idx < FREQ_SIZE; idx++) {
                if (global_freq[idx] > 0) {
                    fprintf(fp, "  %8d | %9llu\n", (int)idx - OFFSET, global_freq[idx]);
                }
            }
        }

        dual_print(fp, "\n=====================================\n");

        if (fp) {
            fclose(fp);
            printf("\nResults saved to: %s\n", OUTPUT_FILE);
        }

        free(global_freq);
    }

    MPI_Finalize();
    return 0;
}