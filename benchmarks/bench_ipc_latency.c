/**
 * bench_ipc_latency.c - REAL IPC latency benchmark
 * 
 * Uses actual ipc_protocol.h framing + warmup + payload size options
 */

#define _GNU_SOURCE
#include "ipc_protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <arpa/inet.h>

#define DEFAULT_SOCKET_PATH "/tmp/beamline-gateway.sock"
#define DEFAULT_REQUESTS 10000
#define WARMUP_REQUESTS 100

static char g_socket_path[256] = DEFAULT_SOCKET_PATH;
static size_t g_payload_size = 64;  /* Default 64 bytes */

/* send_all/recv_all helpers */
static ssize_t send_all(int sock, const void *buf, size_t len) {
    const char *ptr = (const char*)buf;
    size_t remaining = len;
    while (remaining > 0) {
        ssize_t sent = send(sock, ptr, remaining, MSG_NOSIGNAL);
        if (sent < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        ptr += sent;
        remaining -= (size_t)sent;
    }
    return (ssize_t)len;
}

static ssize_t recv_all(int sock, void *buf, size_t len) {
    char *ptr = (char*)buf;
    size_t remaining = len;
    while (remaining > 0) {
        ssize_t received = recv(sock, ptr, remaining, 0);
        if (received < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (received == 0) return 0;
        ptr += received;
        remaining -= (size_t)received;
    }
    return (ssize_t)len;
}

static uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static int compare_uint64(const void *a, const void *b) {
    const uint64_t *va = (const uint64_t*)a;
    const uint64_t *vb = (const uint64_t*)b;
    if (*va < *vb) return -1;
    if (*va > *vb) return 1;
    return 0;
}

static uint64_t percentile(uint64_t *samples, size_t count, double p) {
    if (count == 0) return 0;
    size_t index = (size_t)((double)count * p / 100.0);
    if (index >= count) index = count - 1;
    return samples[index];
}

static int connect_ipc(void) {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) return -1;
    
    struct timeval tv = { .tv_sec = 10, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, g_socket_path, sizeof(addr.sun_path) - 1);
    
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }
    return sock;
}

static int measure_request(int sock, uint64_t *latency_ns) {
    /* Create message with payload */
    char *payload = malloc(g_payload_size);
    memset(payload, 'A', g_payload_size);
    
    ipc_message_t msg = {
        .type = IPC_MSG_PING,
        .payload = payload,
        .payload_len = g_payload_size
    };
    
    char frame_buf[IPC_MAX_FRAME_SIZE];
    ssize_t frame_len = ipc_encode_message(&msg, frame_buf, sizeof(frame_buf));
    free(payload);
    
    if (frame_len < 0) return -1;
    
    /* Measure round-trip */
    uint64_t start = get_time_ns();
    
    if (send_all(sock, frame_buf, (size_t)frame_len) < 0) {
        return -1;
    }
    
    /* Receive response */
    char header[IPC_HEADER_SIZE];
    if (recv_all(sock, header, IPC_HEADER_SIZE) != IPC_HEADER_SIZE) {
        return -1;
    }
    
    /* Validate response header */
    uint32_t resp_len;
    memcpy(&resp_len, header, 4);
    resp_len = ntohl(resp_len);
    
    if (resp_len < IPC_HEADER_SIZE || resp_len > IPC_MAX_FRAME_SIZE) {
        fprintf(stderr, "ERROR: Invalid response frame length: %u\n", resp_len);
        return -1;
    }
    
    /* Validate protocol version */
    uint8_t resp_version = (uint8_t)header[4];
    if (resp_version != IPC_PROTOCOL_VERSION) {
        fprintf(stderr, "ERROR: Invalid response version: got 0x%02x, expected 0x%02x\n",
                resp_version, IPC_PROTOCOL_VERSION);
        return -1;
    }
    
    /* Validate message type - accept RESPONSE_OK or PONG */
    uint8_t resp_type = (uint8_t)header[5];
    if (resp_type != IPC_MSG_RESPONSE_OK && resp_type != IPC_MSG_PONG) {
        fprintf(stderr, "ERROR: Invalid response type: got 0x%02x, expected 0x%02x (RESPONSE_OK) or 0x%02x (PONG)\n",
                resp_type, IPC_MSG_RESPONSE_OK, IPC_MSG_PONG);
        return -1;
    }
    
    size_t payload_len = resp_len - IPC_HEADER_SIZE;
    if (payload_len > 0) {
        char *resp_payload = malloc(payload_len);
        if (recv_all(sock, resp_payload, payload_len) != (ssize_t)payload_len) {
            free(resp_payload);
            return -1;
        }
        free(resp_payload);
    }
    
    uint64_t end = get_time_ns();
    *latency_ns = end - start;
    return 0;
}

int main(int argc, char *argv[]) {
    int num_requests = DEFAULT_REQUESTS;
    int warmup = WARMUP_REQUESTS;
    
    /* Parse args */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            num_requests = atoi(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            /* CLI has highest priority - set immediately */
            strncpy(g_socket_path, argv[i + 1], sizeof(g_socket_path) - 1);
            g_socket_path[sizeof(g_socket_path) - 1] = '\0';
            i++;
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            g_payload_size = (size_t)atoi(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "--no-warmup") == 0) {
            warmup = 0;
        } else if (strcmp(argv[i], "-h") == 0) {
            printf("Usage: %s [-n requests] [-s socket] [-p payload_size] [--no-warmup]\n", argv[0]);
            printf("  -n: Number of requests (default: %d)\n", DEFAULT_REQUESTS);
            printf("  -s: Socket path (default: %s)\n", DEFAULT_SOCKET_PATH);
            printf("  -p: Payload size in bytes (default: 64)\n");
            printf("  --no-warmup: Skip warmup phase\n");
            return 0;
        }
    }
    
    /* Apply ENV only if CLI didn't override (CLI > ENV > default) */
    if (strcmp(g_socket_path, DEFAULT_SOCKET_PATH) == 0) {
        const char *env_socket = getenv("IPC_SOCKET_PATH");
        if (env_socket && env_socket[0]) {
            strncpy(g_socket_path, env_socket, sizeof(g_socket_path) - 1);
            g_socket_path[sizeof(g_socket_path) - 1] = '\0';
        }
    }
    
    printf("IPC Latency Benchmark (REAL PROTOCOL)\n");
    printf("Requests:     %d\n", num_requests);
    printf("Payload Size: %zu bytes\n", g_payload_size);
    printf("Socket:       %s\n", g_socket_path);
    printf("\nConnecting...\n");
    
    int sock = connect_ipc();
    if (sock < 0) {
        fprintf(stderr, "ERROR: Cannot connect to %s\n", g_socket_path);
        fprintf(stderr, "Make sure IPC gateway is running\n");
        return 1;
    }
    
    /* Warmup */
    if (warmup > 0) {
        printf("Warming up (%d requests)...\n", warmup);
        for (int i = 0; i < warmup; i++) {
            uint64_t dummy;
            measure_request(sock, &dummy);
        }
    }
    
    /* Measure */
    printf("Running latency tests...\n");
    uint64_t *latencies = malloc((size_t)num_requests * sizeof(uint64_t));
    int successful = 0;
    
    for (int i = 0; i < num_requests; i++) {
        uint64_t latency;
        if (measure_request(sock, &latency) == 0) {
            latencies[successful++] = latency;
        }
        
        if ((i + 1) % 1000 == 0) {
            printf("  Progress: %d/%d\n", i + 1, num_requests);
        }
    }
    
    close(sock);
    
    if (successful == 0) {
        fprintf(stderr, "ERROR: No successful requests\n");
        free(latencies);
        return 1;
    }
    
    /* Sort and calculate percentiles */
    qsort(latencies, (size_t)successful, sizeof(uint64_t), compare_uint64);
    
    uint64_t min = latencies[0];
    uint64_t max = latencies[successful - 1];
    uint64_t p50 = percentile(latencies, (size_t)successful, 50.0);
    uint64_t p95 = percentile(latencies, (size_t)successful, 95.0);
    uint64_t p99 = percentile(latencies, (size_t)successful, 99.0);
    uint64_t p999 = percentile(latencies, (size_t)successful, 99.9);
    
    uint64_t sum = 0;
    for (int i = 0; i < successful; i++) {
        sum += latencies[i];
    }
    double mean_ns = (double)sum / (double)successful;
    
    printf("\n=== Latency Results ===\n");
    printf("Successful:   %d/%d\n", successful, num_requests);
    printf("Payload Size: %zu bytes\n", g_payload_size);
    printf("Min:          %.3f ms\n", (double)min / 1000000.0);
    printf("Mean:         %.3f ms\n", mean_ns / 1000000.0);
    printf("p50 (median): %.3f ms\n", (double)p50 / 1000000.0);
    printf("p95:          %.3f ms\n", (double)p95 / 1000000.0);
    printf("p99:          %.3f ms\n", (double)p99 / 1000000.0);
    printf("p99.9:        %.3f ms\n", (double)p999 / 1000000.0);
    printf("Max:          %.3f ms\n", (double)max / 1000000.0);
    
    if (p99 <= 10000000ULL) {  /* 10ms */
        printf("\n✅ EXCELLENT: p99 <= 10ms\n");
    } else {
        printf("\n⚠️  WARNING: p99 > 10ms\n");
    }
    
    /* Machine-readable JSON output (last line) */
    printf("{\"benchmark\":\"latency_%zub\",", g_payload_size);
    printf("\"p50_ms\":%.3f,", (double)p50 / 1000000.0);
    printf("\"p95_ms\":%.3f,", (double)p95 / 1000000.0);
    printf("\"p99_ms\":%.3f,", (double)p99 / 1000000.0);
    printf("\"avg_ms\":%.3f,", mean_ns / 1000000.0);
    printf("\"successful\":%d,", successful);
    printf("\"total\":%d,", num_requests);
    printf("\"payload_bytes\":%zu,", g_payload_size);
    printf("\"exit_code\":0}\\n");

    free(latencies);
    return 0;
}
