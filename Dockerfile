# syntax=docker/dockerfile:1.7

ARG UBUNTU_VERSION=24.04

FROM --platform=$BUILDPLATFORM ubuntu:${UBUNTU_VERSION} AS build

ARG DEBIAN_FRONTEND=noninteractive
ARG TEMPIFY_VERSION=0.1.0

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        git \
        libcli11-dev \
        liblua5.4-dev \
        ninja-build \
        pkg-config \
        python3 \
        python3-venv \
    && rm -rf /var/lib/apt/lists/*

RUN python3 -m venv /opt/tempify-build-env \
    && /opt/tempify-build-env/bin/pip install --no-cache-dir cmake==3.31.10

ENV PATH="/opt/tempify-build-env/bin:${PATH}"

WORKDIR /src
COPY . .

RUN cmake -S . -B build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel \
    && cmake --install build --prefix /opt/tempify

FROM ubuntu:${UBUNTU_VERSION} AS runtime

ARG DEBIAN_FRONTEND=noninteractive
ARG TEMPIFY_VERSION=0.1.0
ARG BUILD_DATE

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        liblua5.4-0 \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --create-home --uid 10001 tempify

COPY --from=build /opt/tempify/ /usr/local/

LABEL org.opencontainers.image.title="Tempify" \
      org.opencontainers.image.description="Template engine CLI with bundled ReqPack Tempify packaging support" \
      org.opencontainers.image.version="${TEMPIFY_VERSION}" \
      org.opencontainers.image.created="${BUILD_DATE}"

USER tempify
WORKDIR /work

ENTRYPOINT ["/usr/local/bin/tempify"]
CMD ["-h"]
