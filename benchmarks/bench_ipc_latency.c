/**
 * bench_ipc_latency.c - IPC latency percentile benchmark
 * 
 * Task 20: Measure p50, p95, p99 latency
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#define IPC_SOCKET_PATH "/tmp/ipc_bench.sock"
#define DEFAULT_REQUESTS 10000
#define MAX_SAMPLES 1000000

/**
 * Simple IPC message format
 */
typedef struct {
    uint32_t magic;
    uint32_t length;
    uint8_t type;
    uint8_t reserved[3];
    char payload[256];
} ipc_message_t;

#define IPC_MAGIC 0x49504300

/**
 * Get monotonic time in microseconds
 */
static uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000 + (uint64_t)(ts.tv_nsec / 1000);
}

/**
 * Compare function for qsort
 */
static int compare_uint64(const void *a, const void *b) {
    const uint64_t *va = (const uint64_t*)a;
    const uint64_t *vb = (const uint64_t*)b;
    if (*va < *vb) return -1;
    if (*va > *vb) return 1;
    return 0;
}

/**
 * Calculate percentile
 */
static uint64_t percentile(uint64_t *samples, size_t count, double p) {
    if (count == 0) return 0;
    
    size_t index = (size_t)((double)count * p / 100.0);
    if (index >= count) index = count - 1;
    
    return samples[index];
}

/**
 * Send IPC request and measure latency
 */
static int measure_request(int sock, uint64_t *latency_us) {
    ipc_message_t msg;
    memset(&msg, 0, sizeof(msg));
    
    msg.magic = IPC_MAGIC;
    msg.type = 1;
    snprintf(msg.payload, sizeof(msg.payload), "latency_test");
    msg.length = (uint32_t)strlen(msg.payload);
    
    /* Start timer */
    uint64_t start = get_time_us();
    
    /* Send request */
    if (send(sock, &msg, sizeof(msg), 0) < 0) {
        return -1;
    }
    
    /* Receive response */
    ipc_message_t response;
    if (recv(sock, &response, sizeof(response), 0) < 0) {
        return -1;
    }
    
    /* Stop timer */
    uint64_t end = get_time_us();
    
    *latency_us = end - start;
    return 0;
}

/**
 * Connect to IPC server
 */
static int connect_ipc(void) {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, IPC_SOCKET_PATH, sizeof(addr.sun_path) - 1);
    
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sock);
        return -1;
    }
    
    return sock;
}

int main(int argc, char *argv[]) {
    int num_requests = DEFAULT_REQUESTS;
    
    /* Parse arguments */
    if (argc > 1) {
        num_requests = atoi(argv[1]);
    }
    
    if (num_requests < 1 || num_requests > MAX_SAMPLES) {
        fprintf(stderr, "Invalid request count (1-%d)\n", MAX_SAMPLES);
        return 1;
    }
    
    printf("IPC Latency Benchmark\n");
    printf("Requests: %d\n", num_requests);
    printf("Socket: %s\n", IPC_SOCKET_PATH);
    printf("\nConnecting to IPC server...\n");
    
    /* Connect */
    int sock = connect_ipc();
    if (sock < 0) {
        fprintf(stderr, "Failed to connect to IPC server\n");
        return 1;
    }
    
    printf("Connected. Running latency tests...\n");
    
    /* Allocate sample array */
    uint64_t *latencies = malloc((size_t)num_requests * sizeof(uint64_t));
    if (!latencies) {
        fprintf(stderr, "Failed to allocate memory\n");
        close(sock);
        return 1;
    }
    
    /* Run requests and measure latencies */
    int successful = 0;
    for (int i = 0; i < num_requests; i++) {
        uint64_t latency;
        if (measure_request(sock, &latency) == 0) {
            latencies[successful++] = latency;
        }
        
        if ((i + 1) % 1000 == 0) {
            printf("Progress: %d/%d requests\n", i + 1, num_requests);
        }
    }
    
    close(sock);
    
    if (successful == 0) {
        fprintf(stderr, "No successful requests\n");
        free(latencies);
        return 1;
    }
    
    printf("\nCompleted %d/%d requests successfully\n", successful, num_requests);
    
    /* Sort latencies for percentile calculation */
    qsort(latencies, (size_t)successful, sizeof(uint64_t), compare_uint64);
    
    /* Calculate statistics */
    uint64_t min = latencies[0];
    uint64_t max = latencies[successful - 1];
    uint64_t p50 = percentile(latencies, (size_t)successful, 50.0);
    uint64_t p95 = percentile(latencies, (size_t)successful, 95.0);
    uint64_t p99 = percentile(latencies, (size_t)successful, 99.0);
    uint64_t p999 = percentile(latencies, (size_t)successful, 99.9);
    
    /* Calculate mean */
    uint64_t sum = 0;
    for (int i = 0; i < successful; i++) {
        sum += latencies[i];
    }
    double mean = (double)sum / (double)successful;
    
    /* Print results */
    printf("\n=== IPC Latency Results ===\n");
    printf("Requests:     %d\n", successful);
    printf("Min:          %.3f ms\n", (double)min / 1000.0);
    printf("Mean:         %.3f ms\n", mean / 1000.0);
    printf("p50 (median): %.3f ms\n", (double)p50 / 1000.0);
    printf("p95:          %.3f ms\n", (double)p95 / 1000.0);
    printf("p99:          %.3f ms\n", (double)p99 / 1000.0);
    printf("p99.9:        %.3f ms\n", (double)p999 / 1000.0);
    printf("Max:          %.3f ms\n", (double)max / 1000.0);
    printf("\n");
    
    /* Evaluate against targets */
    if (p99 <= 10000) {  /* 10ms in microseconds */
        printf("✅ EXCELLENT: p99 latency <= 10ms target\n");
    } else {
        printf("⚠️  WARNING: p99 latency > 10ms target\n");
    }
    
    free(latencies);
    return 0;
}
