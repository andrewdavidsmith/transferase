# Use the build env from existing image
FROM andrewdavidsmith/gcc-cmake-boost as build
ARG NUM_JOBS=4
WORKDIR /build
RUN apt-get update
RUN apt-get install -y --no-install-recommends \
    zlib1g-dev \
    ca-certificates
RUN git clone https://github.com/andrewdavidsmith/transferase.git \
    && cd /build/transferase \
    && cmake -B build \
    -DCMAKE_CXX_COMPILER=g++ \
    -DCMAKE_BUILD_TYPE=Distribute \
    && cmake --build build -j${NUM_JOBS} \
    && cmake --install build --prefix /transferase

# Build a light-weight image just with the binary
FROM ubuntu:latest
COPY --from=build /transferase/ /usr/

ENTRYPOINT ["xfr"]
