---
layout: default
title: Compiling JAuto
description: JAuto compilation guide, on *nixes and Docker
parent: Getting Started
nav_order: 0
---
# Compiling JAuto

Clone the <a href="https://github.com/heshiming/jauto" target="_blank">source code repository</a>, or download a release to get a copy of the source code.

JAuto depends on Java development headers and libraries. Thus, a version of JDK (Java Development Kit) must be installed before compilation. Most people favor <a href="https://openjdk.java.net/" target="_blank">OpenJDK</a> over <a href="https://www.oracle.com/java/" target="_blank">Oracle Java</a> since the latter has commercial-use restrictions. JAuto is written in C. It depends on `libc` and requires `gcc` to compile.

JAuto uses <a href="https://cmake.org/" target="_blank">CMake</a> build system.

---

## Building in Linux

In a typical Debian/Ubuntu environment, you can run the following `apt` commands to have all the build dependencies installed:

    $ sudo apt update
    $ sudo apt install default-jdk cmake gcc g++ make libc-dev

Once all the dependencies are set up, follow these steps to compile JAuto:

    tar xfz jauto.tar.gz
    mkdir jauto_build
    cd jauto_build
    cmake ../jauto
    cmake --build .

The built executable is a dynamically loaded library. On Linux, you will get `jauto.so`.

---

## Building in Docker

To build using a <a href="https://www.docker.com/" target="_blank">Docker</a> container, use the <a href="https://hub.docker.com/_/openjdk" target="_blank">OpenJDK official image</a>. Below is an example `dockerfile` to compile JAuto:

    FROM openjdk:18-slim-bullseye AS jauto_build
    USER root
    RUN apt-get update && \
        apt-get install -y --no-install-recommends curl cmake gcc g++ make libc-dev && \
        apt-get clean && \
        rm -rf /var/lib/apt/lists/*
    ENV JAUTO_VER=1.0.0
    RUN curl -Lk https://github.com/heshiming/jauto/archive/refs/tags/v$JAUTO_VER.tar.gz -o /tmp/jauto.tar.gz
    WORKDIR /tmp
    RUN tar xfz jauto.tar.gz && \
        mkdir jauto_build && \
        cd jauto_build && \
        cmake ../jauto-$JAUTO_VER && \
        cmake --build .

You can then copy over `jauto.so` to another image, like this:

    ...
    COPY --from=jauto_build /tmp/jauto_build/jauto.so /opt
    ...
