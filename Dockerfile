FROM ubuntu:22.04

# Install MPI and build tools
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        build-essential \
        openmpi-bin \
        libopenmpi-dev \
        openssh-server \
        openssh-client && \
    rm -rf /var/lib/apt/lists/*

# Allow MPI to run as root (needed inside containers)
ENV OMPI_ALLOW_RUN_AS_ROOT=1
ENV OMPI_ALLOW_RUN_AS_ROOT_CONFIRM=1

# SSH setup for multi-container MPI
RUN mkdir /var/run/sshd && \
    ssh-keygen -t rsa -f /root/.ssh/id_rsa -N "" && \
    cp /root/.ssh/id_rsa.pub /root/.ssh/authorized_keys && \
    echo "StrictHostKeyChecking no" >> /etc/ssh/ssh_config

# Copy source and build
WORKDIR /app
COPY src/ src/
COPY Makefile .
RUN make

# Default: start SSH daemon (for multi-node) or run directly
CMD ["/usr/sbin/sshd", "-D"]