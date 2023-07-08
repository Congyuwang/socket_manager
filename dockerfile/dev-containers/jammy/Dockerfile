FROM --platform=linux/amd64 ubuntu:jammy

# use ustc mirror in China, comment out if you are not in China
#RUN sed -i 's@//.*archive.ubuntu.com@//mirrors.tuna.tsinghua.edu.cn@g' /etc/apt/sources.list

# remove security source to avoid connection error
#RUN sed -i '/^deb http:\/\/security/d' /etc/apt/sources.list

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
        apt-get install -y -q\
        build-essential \
        software-properties-common \
        lsb-core \
        cmake \
        git \
        vim \
        wget \
        curl

# install llvm@16
WORKDIR /root
COPY ./dockerfile/dev-containers/llvm.sh /root
COPY ./update-alternatives-clang.sh /root
RUN chmod +x llvm.sh
RUN ./llvm.sh 16 all
# use tsinghua mirror in China, comment out if you are not in China
#RUN ./llvm.sh 16 all -m https://mirrors.tuna.tsinghua.edu.cn/llvm-apt
RUN ./update-alternatives-clang.sh 16 9999
RUN rm llvm.sh
RUN rm update-alternatives-clang.sh

# install nightly rust
# use ustc mirror in China, comment out if you are not in China
#ARG RUSTUP_UPDATE_ROOT="https://mirrors.ustc.edu.cn/rust-static/rustup"
#ARG RUSTUP_DIST_SERVER="https://mirrors.tuna.tsinghua.edu.cn/rustup"
RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y --default-toolchain nightly

# default sh to bash
RUN chsh -s /bin/bash
ENV HOME /root

ENTRYPOINT ["/bin/bash"]
