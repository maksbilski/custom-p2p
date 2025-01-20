FROM alpine:3.19 AS builder

RUN apk add --no-cache \
    build-base \
    cmake \
    gtest-dev

WORKDIR /app

COPY . .

RUN mkdir -p build && \
    cd build && \
    cmake -DBUILD_TESTS=OFF .. && \
    make

FROM alpine:3.19

RUN apk add --no-cache \
    libstdc++ \
    libgcc

COPY --from=builder /app/build/p2p_resource_sync_app /app/p2p_resource_sync_app

WORKDIR /app

EXPOSE 12345/udp
EXPOSE 12346/tcp

ENTRYPOINT ["./p2p_resource_sync_app"]
