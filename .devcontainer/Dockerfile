FROM ubuntu:rolling

# Ставим всё необходимое для системщины
RUN apt-get update && apt-get install -y \
    clang-21 \
    lldb-21 \
    cmake \
    ninja-build \
    build-essential \
    libicu-dev \
    && apt-get clean