# MPI Distributed Numeric Statistics

A distributed computing project that collects statistics (Frequency, Mean, Max, Min)
from a very large array of unsigned integers using **MPI** and **Docker**.

![C](https://img.shields.io/badge/C-Language-blue)
![MPI](https://img.shields.io/badge/OpenMPI-Distributed-orange)
![Docker](https://img.shields.io/badge/Docker-Containerized-2496ED)

---

## Features

- Process up to **10+ GB** of data with minimal memory (~48 MB per process)
- Chunked processing to stay within memory limits
- Batched MPI frequency reduction
- Configurable array size via command-line arguments
- Results saved to file and printed to console
- Dockerized for easy deployment

---

## Project Structure

```
mpi-distributed-stats/
├── src/
│   ├── config.h           # Constants and configuration
│   ├── utils.h            # Helper functions header
│   ├── utils.c            # Helper functions (dual_print, bytes_to_human)
│   ├── parse_args.h       # Argument parsing header
│   ├── parse_args.c       # Argument parsing (--gb, --mb, --elements, --bytes)
│   ├── frequency.h        # Frequency reduction header
│   ├── frequency.c        # Batched MPI frequency reduction
│   └── main.c             # Entry point
├── output/                # Results output directory
├── Dockerfile             # Multi-container MPI setup
├── Dockerfile.single      # Single-container MPI setup
├── docker-compose.yml     # Multi-node cluster config
├── Makefile               # Build configuration
├── .gitignore
└── README.md
```

---

## Quick Start

### Prerequisites

- [Docker](https://www.docker.com/get-started) installed
- 
## Multi-Node Setup (Docker Compose)
### Build

```bash
docker-compose build ```

### Run

```bash
mkdir -p output

# Test with 2 processes (across containers)
NP=2 ELEMENTS=1000000 docker-compose up --abort-on-container-exit

# Clean up:
docker-compose down -v

# View results
cat output/results.txt
```



---

## How It Works

### Load Distribution

Each MPI process handles `TOTAL_ELEMENTS / num_processes` elements. Data is generated locally on each process — no network transfer needed.

### Memory Management

Instead of allocating the full array, data is processed in chunks of 10M elements (~40 MB). Each process only needs ~48 MB of RAM regardless of total data size.

### Frequency Reduction

The frequency array (1M buckets) is reduced in batches of 250K buckets to avoid memory spikes during MPI communication.

---

## Challenges Addressed

| Challenge | Solution |
|-----------|----------|
| Large integer overflow | `unsigned long long` (64-bit) for sums |
| Frequency array memory | Batched `MPI_Reduce`, only one batch in flight |
| Total data > RAM | Chunked generation and processing |
| Load balancing | Even distribution with remainder handling |

---



## Sample Output

```
=============================================
      Distributed Numeric Statistics         
=============================================

--- Data Info ---
Total elements       : 2684354560
Element size         : 4 bytes (int)
Total data size      : 10.00 GB (10737418240 bytes)
Value range          : [-500000, 500000)

--- Distribution Info ---
MPI processes        : 8
Elements per process : 335544320 (~1.25 GB)
Chunk size           : 10000000 elements (~38.15 MB)
Freq array per proc  : 1000000 buckets (~7.63 MB)
Memory per process   : ~45.78 MB (chunk + freq)

============== Results ==============

--- Size Summary ---
Total data processed : 10.00 GB (2684354560 elements)
Data per process     : 1.25 GB (335544320 elements)
Memory used per proc : ~45.78 MB

--- Statistics ---
Min                  : -500000
Max                  : 499999
Sum                  : -173856429183
Mean                 : -64.766567

--- Performance ---
Total time           : 23.9678 seconds
Throughput           : 0.42 GB/s
Processes used       : 8

```
