# C-Gateway Dockerfile with libnats (nats-io/nats.c)
# NOTE: builds libnats from source at image build time.

FROM alpine:3.20 AS build

RUN apk add --no-cache \
        git \
        cmake \
        make \
        gcc \
        g++ \
        musl-dev \
        openssl-dev \
        jansson-dev

WORKDIR /build

# Fetch and build libnats (official NATS C client)
RUN git clone --depth 1 https://github.com/nats-io/nats.c.git nats.c \
    && cd nats.c \
    && mkdir -p build \
    && cd build \
    && cmake .. \
    && make \
    && make install

# Build c-gateway with USE_NATS_LIB=ON
WORKDIR /app
COPY . .

RUN mkdir -p build \
    && cd build \
    && cmake -DUSE_NATS_LIB=ON .. \
    && make c-gateway

FROM alpine:3.20 AS runtime

RUN apk add --no-cache \
        ca-certificates \
        libstdc++ \
        jansson

COPY --from=build /usr/local/lib /usr/local/lib
COPY --from=build /usr/local/include /usr/local/include
COPY --from=build /app/build/c-gateway /usr/local/bin/c-gateway

ENV GATEWAY_PORT=8080

EXPOSE 8080

CMD ["/usr/local/bin/c-gateway"]
