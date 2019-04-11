FROM ubuntu:16.04
# install mongodb library and g++
RUN apt update \
    && apt install -y cmake wget git pkg-config \
    && wget https://github.com/mongodb/mongo-c-driver/releases/download/1.13.0/mongo-c-driver-1.13.0.tar.gz \
    && tar xzf mongo-c-driver-1.13.0.tar.gz \
    && cd mongo-c-driver-1.13.0 \
    && mkdir cmake-build \
    && cd cmake-build \
    && cmake -DENABLE_AUTOMATIC_INIT_AND_CLEANUP=OFF .. \
    && make && make install \
    && git clone https://github.com/mongodb/mongo-cxx-driver.git --branch releases/stable --depth 1 \
    && cd mongo-cxx-driver/build \
    && apt install -y g++ \
    && cmake -DBSONCXX_POLY_USE_MNMLSTC=1 -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local .. \
    && make EP_mnmlstc_core \
    && make && make install \
    && cd / && rm -rf mongo* \
    && apt purge -y --auto-remove cmake wget git \
    && rm -rf /var/lib/apt/lists/*
COPY judge /judge/judge