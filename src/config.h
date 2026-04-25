#ifndef CONFIG_H
#define CONFIG_H

#define MAX_VAL          1000000U       /* values in [0, MAX_VAL)        */
#define FREQ_SIZE        MAX_VAL        /* one bucket per possible value */
#define CHUNK_SIZE       10000000LL     /* process 10M elements at a time */
#define FREQ_BATCH       250000         /* buckets reduced per round     */
#define OUTPUT_FILE      "/app/output/results.txt"
#define DEFAULT_ELEMENTS 2684354560ULL  /* 10 GB default                 */

#endif