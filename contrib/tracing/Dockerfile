#for kernel libraries
FROM linuxkit/kernel:4.15 as ksrc

#for tapyrus
FROM --platform=$TARGETPLATFORM tapyrus/builder:v0.2.0 as builder
ENV LC_ALL C.UTF-8

ENV BUILD_PACKAGES "build-essential libtool autotools-dev automake pkg-config bsdmainutils curl git ca-certificates ccache g++ make cmake gdb gdbserver rsync zip openssh-server lldb ssh"

ENV TAPYRUS_PACKAGES "python3-zmq libevent-dev libboost-system-dev libboost-filesystem-dev libboost-chrono-dev libboost-test-dev libboost-thread-dev libdb5.3++-dev libminiupnpc-dev libzmq3-dev libqrencode-dev qtbase5-dev-tools qt5-qmake qtbase5-dev libqwt-qt5-dev qttools5-dev-tools systemtap-sdt-dev"

RUN apt-get update && \
    apt-get install --no-install-recommends --no-upgrade -qq $PACKAGES $BUILD_PACKAGES

RUN apt-get update && \
    apt-get install --no-install-recommends --no-upgrade -qq $PACKAGES $TAPYRUS_PACKAGES

#for tapyrus source code
WORKDIR /tapyrus-core
COPY . .

#env for tapyrus compilation
ENV TAPYRUS_CONFIG "--disable-tests --disable-bench --disable-dependency-tracking  --bindir=/tapyrus-core/dist/bin  --libdir=/tapyrus-core/dist/lib --enable-zmq --enable-reduce-exports --with-incompatible-bdb --with-gui=yes --enable-cxx --disable-shared --disable-replication --with-pic CPPFLAGS=-DDEBUG_LOCKORDER"

#compilation disabled during the docker image build
#RUN ./autogen.sh && \
#    if [ "$TARGETARCH" = "arm64" ]; then BUILD_HOST="aarch64-linux-gnu"; else #BUILD_HOST="x86_64-pc-linux-gnu"; fi && \
#    ./configure --prefix=/tapyrus-core/depends/$BUILD_HOST  $TAPYRUS_CONFIG && \
#    make -j"$(($(nproc)+1))" && \
#    make install

ENV BPFTRACE_VERSION v0.15.0

#kernel dev modules
WORKDIR /kernel
COPY --from=ksrc /kernel-dev.tar .
RUN tar xf kernel-dev.tar

#bpftrace
WORKDIR /work
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

RUN echo 'root:root' | chpasswd \
    && sed -i 's/#PermitRootLogin prohibit-password/PermitRootLogin yes/' /etc/ssh/sshd_config \
    && sed 's@session\s*required\s*pam_loginuid.so@session optional pam_loginuid.so@g' -i /etc/pam.d/sshd

#for tapyrus runtime
ENV DATA_DIR='/var/lib/tapyrus' \
    CONF_DIR='/etc/tapyrus'
RUN mkdir ${DATA_DIR} && mkdir ${CONF_DIR}

# p2p port (Production/Development) rpc port (Production/Development)
EXPOSE 2357 12383 2377 12381 22

COPY entrypoint.sh /usr/local/bin/

VOLUME ["$DATA_DIR"]

ENTRYPOINT ["../entrypoint.sh"]

CMD ["/usr/sbin/sshd", "-D"]