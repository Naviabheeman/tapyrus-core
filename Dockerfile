FROM linuxkit/kernel:5.15.110-9c153b657f4a39023d01eb2ff395c58f1bb331da-amd64 as ksrc
FROM --platform=$TARGETPLATFORM tapyrus/builder:v0.3.0 as builder
ARG TARGETARCH

ENV LC_ALL C.UTF-8
ENV TAPYRUS_CONFIG "--disable-tests --disable-bench --disable-dependency-tracking  --bindir=/tapyrus-core/dist/bin  --libdir=/tapyrus-core/dist/lib --enable-zmq --enable-reduce-exports --with-incompatible-bdb --with-gui=no --enable-cxx --disable-shared --disable-replication --with-pic CPPFLAGS=-DDEBUG_LOCKORDER"

WORKDIR /tapyrus-core
COPY . .

RUN ./autogen.sh && \
    if [ "$TARGETARCH" = "arm64" ]; then BUILD_HOST="aarch64-linux-gnu"; else BUILD_HOST="x86_64-pc-linux-gnu"; fi && \
    ./configure --prefix=/tapyrus-core/depends/$BUILD_HOST  $TAPYRUS_CONFIG && \
    make -j"$(($(nproc)+1))" && \
    make install

FROM ubuntu:20.04

COPY --from=builder /tapyrus-core/dist/bin/* /usr/local/bin/

ENV DATA_DIR='/var/lib/tapyrus' \
    CONF_DIR='/etc/tapyrus'
RUN mkdir ${DATA_DIR} && mkdir ${CONF_DIR}

# p2p port (Production/Development) rpc port (Production/Development)
EXPOSE 2357 12383 2377 12381

COPY entrypoint.sh /usr/local/bin/

VOLUME ["$DATA_DIR"]

#for linux-headers
WORKDIR /kernel
COPY  --from=ksrc /kernel-dev.tar .
RUN tar xf kernel-dev.tar

#bpftrace
ENV BPFTRACE_VERSION v0.15.0
WORKDIR /bpftrace
RUN wget https://github.com/iovisor/bpftrace/releases/download/${BPFTRACE_VERSION}/bpftrace && \
    chmod +x bpftrace && \
    mv bpftrace /bin && \
    mkdir -p /tracing && \
    wget https://github.com/iovisor/bpftrace/archive/${BPFTRACE_VERSION}.zip && \
    unzip ${BPFTRACE_VERSION}.zip && \
    cp -r bpftrace*/tools /tracing &&\
    mv /kernel/usr/src/linux-headers* /kernel/usr/src/linux-headers

ENV BPFTRACE_KERNEL_SOURCE=/kernel/usr/src/linux-headers

ENTRYPOINT ["entrypoint.sh"]
CMD ["tapyrusd -datadir=${DATA_DIR} -conf=${CONF_DIR}/tapyrus.conf"]

#run this container with --privileged option to use bpf trace scripts