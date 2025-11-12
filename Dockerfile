# -------- Stage 1: Builder --------
FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
    qt6-base-dev qt6-base-dev-tools qt6-tools-dev qt6-tools-dev-tools qt6-websockets-dev \
    libqt6sql6 libqt6sql6-sqlite \
    libssl-dev libcurl4-openssl-dev libjsoncpp-dev \
    build-essential autoconf libtool git unzip ca-certificates wget xz-utils patchelf \
    && apt-get clean

# Dein Projekt bauen
WORKDIR /src
RUN git clone --branch main --single-branch https://github.com/wiesche89/grin-node-controller.git .
RUN qmake6 grin-node-controller.pro && make -j"$(nproc)"
