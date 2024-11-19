# Use the build env from existing image
FROM andrewdavidsmith/gcc-cmake-boost as build
ARG NUM_JOBS=4
WORKDIR /build
RUN apt-get update
RUN apt-get install -y --no-install-recommends \
    zlib1g-dev \
    ca-certificates
RUN git clone https://github.com/andrewdavidsmith/mxe.git && \
    cd /build/mxe && \
    cmake -B Release \
    -DMXE_USE_STATIC_LIBS=on \
    -DCMAKE_VERBOSE_MAKEFILE=on \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=on \
    -DCMAKE_CXX_COMPILER=g++ \
    -DCMAKE_BUILD_TYPE=Release && \
    cmake --build Release -j${NUM_JOBS} && \
    cmake --install Release --prefix /build

# Build a light-weight image just with the binary
FROM ubuntu:latest
COPY --from=build /build/bin /usr/bin

ENTRYPOINT ["mxe"]
