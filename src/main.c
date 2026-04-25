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

    /* ---- Size calculations ---- */
    double total_data_bytes   = (double)TOTAL_ELEMENTS * sizeof(unsigned int);
    double local_data_bytes   = (double)local_count * sizeof(unsigned int);
    double chunk_mem_mb       = (double)CHUNK_SIZE * sizeof(unsigned int) / (1024.0 * 1024.0);
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
        printf("Element size         : %lu bytes (unsigned int)\n", sizeof(unsigned int));
        printf("Total data size      : %s (%.0f bytes)\n", total_size_str, total_data_bytes);
        printf("Value range          : [0, %u)\n\n", MAX_VAL);
        printf("--- Distribution Info ---\n");
        printf("MPI processes        : %d\n", size);
        printf("Elements per process : %lld (~%s)\n", local_count, local_size_str);
        printf("Chunk size           : %lld elements (~%.2f MB)\n", (long long)CHUNK_SIZE, chunk_mem_mb);
        printf("Freq array per proc  : %u buckets (~%.2f MB)\n", (unsigned int)FREQ_SIZE, freq_mem_mb);
        printf("Memory per process   : ~%.2f MB (chunk + freq)\n\n", mem_per_process_mb);
    }

    /* ---- 2. Local statistics variables ---- */
    unsigned int       local_min = UINT_MAX;
    unsigned int       local_max = 0;
    unsigned long long local_sum = 0ULL;

    unsigned long long *local_freq =
        (unsigned long long *)calloc(FREQ_SIZE, sizeof(unsigned long long));
    if (!local_freq) {
        fprintf(stderr, "[%d] Cannot allocate freq array\n", rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    long long buf_size = (local_count < CHUNK_SIZE) ? local_count : CHUNK_SIZE;
    unsigned int *buf = (unsigned int *)malloc(buf_size * sizeof(unsigned int));
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

        for (long long i = 0; i < this_chunk; i++) {
            buf[i] = (unsigned int)(rand() % MAX_VAL);
        }

        for (long long i = 0; i < this_chunk; i++) {
            unsigned int v = buf[i];
            if (v < local_min) local_min = v;
            if (v > local_max) local_max = v;
            local_sum += v;
            local_freq[v]++;
        }

        remaining -= this_chunk;

        if (rank == 0) {
            double done = (double)(local_count - remaining);
            double pct  = 100.0 * done / (double)local_count;
            double done_gb = done * sizeof(unsigned int) / (1024.0*1024.0*1024.0);
            printf("\r  [Rank 0] Progress: %.1f%% (%.2f / %.2f GB processed)",
                   pct, done_gb, local_data_gb);
            fflush(stdout);
        }
    }

    if (rank == 0) printf("\n\n");
    free(buf);

    /* ---- 4. Global reductions ---- */
    unsigned int       global_min;
    unsigned int       global_max;
    unsigned long long global_sum;

    MPI_Reduce(&local_min, &global_min, 1, MPI_UNSIGNED,           MPI_MIN, 0, MPI_COMM_WORLD);
    MPI_Reduce(&local_max, &global_max, 1, MPI_UNSIGNED,           MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&local_sum, &global_sum, 1, MPI_UNSIGNED_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);

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
        dual_print(fp, "Element size         : %lu bytes (unsigned int)\n", sizeof(unsigned int));
        dual_print(fp, "Total data size      : %s (%.0f bytes)\n", total_size_str, total_data_bytes);
        dual_print(fp, "Value range          : [0, %u)\n\n", MAX_VAL);

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
        dual_print(fp, "Min                  : %u\n",   global_min);
        dual_print(fp, "Max                  : %u\n",   global_max);
        dual_print(fp, "Sum                  : %llu\n", global_sum);
        dual_print(fp, "Mean                 : %.6f\n", mean);

        dual_print(fp, "\n--- Performance ---\n");
        dual_print(fp, "Total time           : %.4f seconds\n", elapsed);
        dual_print(fp, "Throughput           : %.2f GB/s\n", throughput_gb);
        dual_print(fp, "Processes used       : %d\n", size);

        /* ---- Top 10 ---- */
        dual_print(fp, "\n--- Top 10 Most Frequent Values ---\n");
        dual_print(fp, "  Value   |   Count\n");
        dual_print(fp, "----------+-----------\n");

        unsigned int  top_val[10];
        unsigned long long top_cnt[10];
        memset(top_cnt, 0, sizeof(top_cnt));

        for (unsigned int v = 0; v < FREQ_SIZE; v++) {
            int mi = 0;
            for (int k = 1; k < 10; k++)
                if (top_cnt[k] < top_cnt[mi]) mi = k;
            if (global_freq[v] > top_cnt[mi]) {
                top_cnt[mi] = global_freq[v];
                top_val[mi] = v;
            }
        }
        for (int i = 0; i < 9; i++)
            for (int j = i + 1; j < 10; j++)
                if (top_cnt[j] > top_cnt[i]) {
                    unsigned long long tc = top_cnt[i]; top_cnt[i] = top_cnt[j]; top_cnt[j] = tc;
                    unsigned int       tv = top_val[i]; top_val[i] = top_val[j]; top_val[j] = tv;
                }
        for (int i = 0; i < 10; i++)
            dual_print(fp, "  %7u | %9llu\n", top_val[i], top_cnt[i]);

        /* Full frequency to file only */
        if (fp) {
            fprintf(fp, "\n--- Full Frequency Array ---\n");
            fprintf(fp, "  Value   |   Count\n");
            fprintf(fp, "----------+-----------\n");
            for (unsigned int v = 0; v < FREQ_SIZE; v++) {
                if (global_freq[v] > 0) {
                    fprintf(fp, "  %7u | %9llu\n", v, global_freq[v]);
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