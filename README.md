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
├── hostfile               # MPI host configuration
├── .gitignore
├── LICENSE
└── README.md
```

---

## Quick Start

### Prerequisites

- [Docker](https://www.docker.com/get-started) installed

### Build

```bash
docker build -f Dockerfile.single -t mpi-stats .
```

### Run

```bash
mkdir -p output

# Default: 10 GB with 4 processes
docker run --rm -v $(pwd)/output:/app/output mpi-stats

# Custom size examples
docker run --rm -v $(pwd)/output:/app/output mpi-stats -np 4 /app/stats --gb 10
docker run --rm -v $(pwd)/output:/app/output mpi-stats -np 4 /app/stats --mb 500
docker run --rm -v $(pwd)/output:/app/output mpi-stats -np 8 /app/stats --gb 1
docker run --rm -v $(pwd)/output:/app/output mpi-stats -np 4 /app/stats --elements 100000000

# View results
cat output/results.txt
```

### Size Options

| Flag | Example | Description |
|------|---------|-------------|
| `--gb <N>` | `--gb 10` | Size in gigabytes |
| `--mb <N>` | `--mb 500` | Size in megabytes |
| `--elements <N>` | `--elements 100000000` | Exact element count |
| `--bytes <N>` | `--bytes 10737418240` | Exact byte count |
| *(no args)* | | Default: 10 GB |

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

## Multi-Node Setup (Docker Compose)

```bash
docker-compose up --build
```

This launches 3 containers (1 master + 2 workers) with 2 processes each — **6 total MPI processes**.

---

## Sample Output

```
=============================================
      Distributed Numeric Statistics
=============================================

--- Data Info ---
Total elements       : 2684354560
Element size         : 4 bytes (unsigned int)
Total data size      : 10.00 GB (10737418240 bytes)
Value range          : [0, 1000000)

--- Distribution Info ---
MPI processes        : 4
Elements per process : 671088640 (~2.50 GB)
Chunk size           : 10000000 elements (~38.15 MB)
Freq array per proc  : 1000000 buckets (~7.63 MB)
Memory per process   : ~45.78 MB (chunk + freq)

============== Results ==============

--- Statistics ---
Min                  : 0
Max                  : 999999
Sum                  : 1341856742338560
Mean                 : 499979.238476

--- Performance ---
Total time           : 45.1234 seconds
Throughput           : 0.22 GB/s
Processes used       : 4
```

---

## Build Without Docker (Local MPI)

```bash
# Install MPI (Ubuntu/Debian)
sudo apt-get install build-essential openmpi-bin libopenmpi-dev

# Build
make

# Run
mpirun -np 4 ./stats --gb 10
```
