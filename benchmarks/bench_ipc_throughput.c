/**
 * bench_ipc_throughput.c - REAL IPC throughput benchmark
 * 
 * Uses actual ipc_protocol.h framing
 */

#define _GNU_SOURCE
#include "ipc_protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdatomic.h>
#include <errno.h>
#include <arpa/inet.h>

/* Default socket path - can override with env or -s */
#define DEFAULT_SOCKET_PATH "/tmp/beamline-gateway.sock"
#define DEFAULT_DURATION 10
#define DEFAULT_THREADS 4
#define DEFAULT_WARMUP_REQUESTS 100

static void print_usage(const char *prog) {
    printf("Usage: %s [OPTIONS]\n", prog);
    printf("\nIPC Throughput Benchmark (REAL PROTOCOL)\n");
    printf("\nOptions:\n");
    printf("  -d <seconds>   Duration in seconds (default: %d)\n", DEFAULT_DURATION);
    printf("  -t <threads>   Number of threads (default: %d)\n", DEFAULT_THREADS);
    printf("  -p <bytes>     Payload size in bytes (default: 2)\n");
    printf("  -s <path>      Socket path (default: %s)\n", DEFAULT_SOCKET_PATH);
    printf("  -h             Show this help\n");
    printf("\nSocket Priority: CLI (-s) > ENV (IPC_SOCKET_PATH) > default\n");
    printf("Warmup: %d requests before measurement\n", DEFAULT_WARMUP_REQUESTS);
    printf("\n");
}

/* Atomic counters */
static atomic_ulong g_requests_sent = 0;
static atomic_ulong g_requests_completed = 0;
static atomic_ulong g_requests_failed = 0;
static atomic_int g_running = 1;

/* Socket path (configurable) */
static char g_socket_path[256] = DEFAULT_SOCKET_PATH;
static size_t g_payload_size = 2;  /* Default: "{}" */
static int g_warmup_requests = DEFAULT_WARMUP_REQUESTS;

/**
 * send_all - ensure all bytes are sent
 */
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

/**
 * recv_all - ensure all bytes are received
 */
static ssize_t recv_all(int sock, void *buf, size_t len) {
    char *ptr = (char*)buf;
    size_t remaining = len;
    
    while (remaining > 0) {
        ssize_t received = recv(sock, ptr, remaining, 0);
        if (received < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (received == 0) return 0;  /* EOF */
        ptr += received;
        remaining -= (size_t)received;
    }
    return (ssize_t)len;
}

/**
 * Send IPC request using real protocol
 */
static int send_ipc_request(int sock) {
    /* Create PING message with configurable payload */
    char *payload = malloc(g_payload_size);
    if (!payload) return -1;
    memset(payload, 'A', g_payload_size);
    
    ipc_message_t msg = {
        .type = IPC_MSG_PING,
        .payload = payload,
        .payload_len = g_payload_size
    };
    
    /* Encode to frame */
    char frame_buf[IPC_MAX_FRAME_SIZE];
    ssize_t frame_len = ipc_encode_message(&msg, frame_buf, sizeof(frame_buf));
    free(payload);
    
    if (frame_len < 0) {
        return -1;
    }
    
    /* Send frame */
    if (send_all(sock, frame_buf, (size_t)frame_len) < 0) {
        return -1;
    }
    
    /* Receive response header */
    char header[IPC_HEADER_SIZE];
    if (recv_all(sock, header, IPC_HEADER_SIZE) != IPC_HEADER_SIZE) {
        return -1;
    }
    
    /* Parse length */
    uint32_t frame_length;
    memcpy(&frame_length, header, 4);
    frame_length = ntohl(frame_length);
    
    if (frame_length < IPC_HEADER_SIZE || frame_length > IPC_MAX_FRAME_SIZE) {
        return -1;
    }
    
    /* Receive remaining payload */
    char *response_frame = malloc(frame_length);
    if (!response_frame) return -1;
    memcpy(response_frame, header, IPC_HEADER_SIZE); // Copy header to the full frame buffer
    
    size_t payload_len = frame_length - IPC_HEADER_SIZE;
    if (payload_len > 0) {
        if (recv_all(sock, response_frame + IPC_HEADER_SIZE, payload_len) != (ssize_t)payload_len) {
            free(response_frame);
            return -1;
        }
    }
    
    /* Decode response and validate */
    ipc_message_t response_msg;
    ssize_t decoded_len = ipc_decode_message(response_frame, frame_length, &response_msg);
    free(response_frame); // Free the buffer used for receiving the frame
    
    if (decoded_len < 0) {
        return -1; // Failed to decode response
    }
    
    if (response_msg.type != IPC_MSG_PONG) {
        // Optionally free response_msg.payload if it was dynamically allocated by ipc_decode_message
        // For this benchmark, we don't care about the content, just the type.
        // Assuming ipc_decode_message manages its own payload memory or it's on stack.
        return -1; // Expected PONG, got something else
    }
    
    // If ipc_decode_message allocates payload, it should be freed here.
    // Assuming for now it's not dynamically allocated or is handled internally.
    // If ipc_decode_message returns a pointer to a dynamically allocated payload,
    // it should be freed: if (response_msg.payload) free(response_msg.payload);
    
    return 0;
}

/**
 * Connect to IPC server
 */
static int connect_ipc(void) {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        return -1;
    }
    
    /* Set receive timeout */
    struct timeval tv = { .tv_sec = 5, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, g_socket_path, sizeof(addr.sun_path) - 1);
    addr.sun_path[sizeof(addr.sun_path) - 1] = \0;  /* Suppress -Werror=stringop-truncation */
    
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }
    
    return sock;
}

/**
 * Worker thread
 */
static void* worker_thread(void* arg) {
    (void)arg;
    
    /* Connect */
    int sock = connect_ipc();
    if (sock < 0) {
        fprintf(stderr, "[worker] Failed to connect\n");
        return NULL;
    }
    
    /* Send requests */
    while (atomic_load(&g_running)) {
        atomic_fetch_add(&g_requests_sent, 1);
        
        if (send_ipc_request(sock) == 0) {
            atomic_fetch_add(&g_requests_completed, 1);
        } else {
            atomic_fetch_add(&g_requests_failed, 1);
        }
    }
    
    close(sock);
    return NULL;
}

int main(int argc, char *argv[]) {
    int duration = DEFAULT_DURATION;
    int num_threads = DEFAULT_THREADS;
    
    /* Parse arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            duration = atoi(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            num_threads = atoi(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            strncpy(g_socket_path, argv[i + 1], sizeof(g_socket_path) - 1);
            g_socket_path[sizeof(g_socket_path) - 1] = '\0';
            i++;
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            g_payload_size = (size_t)atoi(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-h") == 0) {
            printf("Usage: %s [-d duration] [-t threads] [-s socket]\n", argv[0]);
            printf("  -d: Duration in seconds (default: %d)\n", DEFAULT_DURATION);
            printf("  -t: Number of threads (default: %d)\n", DEFAULT_THREADS);
            printf("  -s: Socket path (default: %s or $IPC_SOCKET_PATH)\n", DEFAULT_SOCKET_PATH);
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
    
    printf("IPC Throughput Benchmark (REAL PROTOCOL)\n");
    printf("Duration: %d seconds, Threads: %d\n", duration, num_threads);
    printf("Payload: %zu bytes\n", g_payload_size);
    printf("Warmup: %d requests\n", g_warmup_requests);
    printf("Socket: %s\n", g_socket_path);
    printf("\n");
    
    /* Warmup phase */
    if (g_warmup_requests > 0) {
        printf("=== Warmup Phase ===\n");
        int warmup_sock = connect_ipc();
        if (warmup_sock < 0) {
            fprintf(stderr, "Failed to connect for warmup\n");
            return 1;
        }
        
        for (int i = 0; i < g_warmup_requests; i++) {
            send_ipc_request(warmup_sock);
        }
        close(warmup_sock);
        printf("Warmup complete: %d requests\n", g_warmup_requests);
        printf("\n");
    }
    
    /* Spawn worker threads */
    printf("Starting benchmark...\n");
    
    /* Create threads */
    pthread_t *threads = malloc((size_t)num_threads * sizeof(pthread_t));
    for (int i = 0; i < num_threads; i++) {
        if (pthread_create(&threads[i], NULL, worker_thread, NULL) != 0) {
            fprintf(stderr, "Failed to create thread\n");
            return 1;
        }
    }
    
    /* Run */
    sleep((unsigned int)duration);
    atomic_store(&g_running, 0);
    
    /* Wait */
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    free(threads);
    
    /* Results */
    unsigned long sent = atomic_load(&g_requests_sent);
    unsigned long completed = atomic_load(&g_requests_completed);
    unsigned long failed = atomic_load(&g_requests_failed);
    
    double throughput = (double)completed / (double)duration;
    double success_rate = sent > 0 ? ((double)completed / (double)sent * 100.0) : 0.0;
    
    printf("\n=== Results ===\n");
    printf("Duration:       %d seconds\n", duration);
    printf("Threads:        %d\n", num_threads);
    printf("Sent:           %lu\n", sent);
    printf("Completed:      %lu\n", completed);
    printf("Failed:         %lu\n", failed);
    printf("Throughput:     %.0f req/sec\n", throughput);
    printf("Success Rate:   %.2f%%\n", success_rate);
    
    if (throughput >= 5000) {
        printf("\n✅ EXCELLENT: >= 5,000 req/sec\n");
    } else if (throughput >= 1000) {
        printf("\n✓ GOOD: >= 1,000 req/sec\n");
    } else {
        printf("\n⚠️  WARNING: < 1,000 req/sec\n");
    }
    
    /* Machine-readable JSON output (last line) */
    printf("{\"benchmark\":\"throughput_%zub\",", g_payload_size);
    printf("\"rps\":%.0f,", throughput);
    printf("\"requests\":%lu,", sent);
    printf("\"completed\":%lu,", completed);
    printf("\"failed\":%lu,", failed);
    printf("\"duration_s\":%d,", duration);
    printf("\"threads\":%d,", num_threads);
    printf("\"payload_bytes\":%zu,", g_payload_size);
    printf("\"exit_code\":0}\\n");

    return 0;
}
