#for kernel libraries

FROM --platform=$TARGETPLATFORM tapyrus/builder:v0.2.0 as builder
ARG TARGETARCH

ENV LC_ALL C.UTF-8
ENV TAPYRUS_CONFIG "--disable-tests --disable-bench --disable-dependency-tracking  --bindir=/tapyrus-core/dist/bin  --libdir=/tapyrus-core/dist/lib --enable-zmq --enable-reduce-exports --with-incompatible-bdb --with-gui=no --with-usdt=yes CPPFLAGS=-DDEBUG_LOCKORDER"

ENV TRACE_PACKAGES "cmake gdb gdbserver rsync zip openssh-server lldb ssh systemtap-sdt-dev bpfcc-tools python3-bpfcc"

RUN apt-get update && \
    apt-get install --no-install-recommends --no-upgrade -qq $TRACE_PACKAGES

WORKDIR /tapyrus-core
COPY . .

RUN ./autogen.sh && \
    if [ "$TARGETARCH" = "arm64" ]; then BUILD_HOST="aarch64-linux-gnu"; else BUILD_HOST="x86_64-pc-linux-gnu"; fi && \
    ./configure --prefix=/tapyrus-core/depends/$BUILD_HOST --enable-cxx --disable-shared --disable-replication --with-pic --with-incompatible-bdb $TAPYRUS_CONFIG && \
    make -j"$(($(nproc)+1))" && \
    make install

FROM ubuntu:18.04

COPY --from=builder /tapyrus-core/dist/bin/* /usr/local/bin/

#source code
WORKDIR /tapyrus-core
COPY . .

#bpftrace
ENV BPFTRACE_VERSION v0.15.0
WORKDIR /bpftrace
RUN apt-get update && apt-get install -y \
    wget vim tmux git binutils unzip && \
    wget https://github.com/iovisor/bpftrace/releases/download/${BPFTRACE_VERSION}/bpftrace && \
    chmod +x bpftrace && \
    mv bpftrace /bin && \
    mkdir -p /tracing && \
    wget https://github.com/iovisor/bpftrace/archive/${BPFTRACE_VERSION}.zip && \
    unzip ${BPFTRACE_VERSION}.zip && \
    cp -r bpftrace*/tools /tracing &&\
    mv /kernel/usr/src/linux-headers* /kernel/usr/src/linux-headers

ENV BPFTRACE_KERNEL_SOURCE=/kernel/usr/src/linux-headers

# configure SSH for communication with Visual Studio 
RUN mkdir -p /var/run/sshd


ENV DATA_DIR='/var/lib/tapyrus' \
    CONF_DIR='/etc/tapyrus'
RUN mkdir ${DATA_DIR} && mkdir ${CONF_DIR}

# p2p port (Production/Development) rpc port (Production/Development)
EXPOSE 2357 12383 2377 12381

COPY entrypoint.sh /usr/local/bin/

VOLUME ["$DATA_DIR"]

ENTRYPOINT ["entrypoint.sh"]
CMD ["tapyrusd -datadir=${DATA_DIR} -conf=${CONF_DIR}/tapyrus.conf"]
