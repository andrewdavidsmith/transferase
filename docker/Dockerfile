# Use the build env from existing image
FROM andrewdavidsmith/gcc-cmake-boost as build_mc16
ARG NUM_JOBS=4
WORKDIR /build
RUN apt-get update
RUN apt-get install -y --no-install-recommends \
    zlib1g-dev
RUN --mount=type=secret,id=github_token \
    GITHUB_TOKEN=$(cat /run/secrets/github_token) && \
    git clone https://oauth2:${GITHUB_TOKEN}@github.com/andrewdavidsmith/mc16.git && \
    cd /build/mc16 && \
    cmake -B Release \
    -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DCMAKE_CXX_COMPILER=g++ \
    -DCMAKE_BUILD_TYPE=Release && \
    cmake --build Release -j${NUM_JOBS} && \
    cmake --install Release --prefix /build

# Build a light-weight image just with the binary
FROM alpine:latest
COPY --from=build_mc16 /build/bin /usr/bin

ENTRYPOINT ["mc16"]
