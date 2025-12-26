/**
 * bench_ipc_throughput.c - IPC throughput benchmark
 * 
 * Task 20: Measure sustained requests/second
 */

#define _GNU_SOURCE
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

#define IPC_SOCKET_PATH "/tmp/ipc_bench.sock"
#define DEFAULT_DURATION 10  /* seconds */
#define DEFAULT_THREADS 4

/* Atomic counters */
static atomic_ulong g_requests_sent = 0;
static atomic_ulong g_requests_completed = 0;
static atomic_ulong g_requests_failed = 0;
static atomic_int g_running = 1;

/**
 * Simple IPC message format (minimal for benchmark)
 */
typedef struct {
    uint32_t magic;      /* 0x49504300 */
    uint32_t length;     /* Payload length */
    uint8_t type;        /* Message type */
    uint8_t reserved[3];
    char payload[256];
} ipc_message_t;

#define IPC_MAGIC 0x49504300

/**
 * Send a simple IPC request
 */
static int send_ipc_request(int sock) {
    ipc_message_t msg;
    memset(&msg, 0, sizeof(msg));
    
    msg.magic = IPC_MAGIC;
    msg.type = 1;  /* REQUEST */
    snprintf(msg.payload, sizeof(msg.payload), "benchmark");
    msg.length = (uint32_t)strlen(msg.payload);
    
    /* Send request */
    ssize_t sent = send(sock, &msg, sizeof(msg), 0);
    if (sent < 0) {
        return -1;
    }
    
    /* Receive response (simple echo) */
    ipc_message_t response;
    ssize_t received = recv(sock, &response, sizeof(response), 0);
    if (received < 0) {
        return -1;
    }
    
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

/**
 * Worker thread - sends requests continuously
 */
static void* worker_thread(void* arg) {
    (void)arg;
    
    /* Connect to IPC server */
    int sock = connect_ipc();
    if (sock < 0) {
        fprintf(stderr, "[worker] Failed to connect to IPC server\n");
        return NULL;
    }
    
    /* Send requests until stopped */
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

/**
 * Print results
 */
static void print_results(int duration, int num_threads) {
    unsigned long sent = atomic_load(&g_requests_sent);
    unsigned long completed = atomic_load(&g_requests_completed);
    unsigned long failed = atomic_load(&g_requests_failed);
    
    double throughput = (double)completed / (double)duration;
    double success_rate = sent > 0 ? ((double)completed / (double)sent * 100.0) : 0.0;
    
    printf("\n");
    printf("=== IPC Throughput Benchmark Results ===\n");
    printf("Duration:          %d seconds\n", duration);
    printf("Threads:           %d\n", num_threads);
    printf("Total Sent:        %lu requests\n", sent);
    printf("Completed:         %lu requests\n", completed);
    printf("Failed:            %lu requests\n", failed);
    printf("Throughput:        %.0f req/sec\n", throughput);
    printf("Success Rate:      %.2f%%\n", success_rate);
    printf("\n");
    
    if (throughput < 1000) {
        printf("⚠️  WARNING: Throughput below 1,000 req/sec\n");
    } else if (throughput >= 5000) {
        printf("✅ EXCELLENT: Throughput >= 5,000 req/sec target\n");
    } else {
        printf("✓ GOOD: Throughput acceptable\n");
    }
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
        } else if (strcmp(argv[i], "-h") == 0) {
            printf("Usage: %s [-d duration] [-t threads]\n", argv[0]);
            printf("  -d: Duration in seconds (default: %d)\n", DEFAULT_DURATION);
            printf("  -t: Number of threads (default: %d)\n", DEFAULT_THREADS);
            return 0;
        }
    }
    
    if (duration < 1) duration = 1;
    if (num_threads < 1) num_threads = 1;
    
    printf("IPC Throughput Benchmark\n");
    printf("Duration: %d seconds, Threads: %d\n", duration, num_threads);
    printf("Socket: %s\n", IPC_SOCKET_PATH);
    printf("\nStarting benchmark...\n");
    
    /* Create worker threads */
    pthread_t *threads = malloc((size_t)num_threads * sizeof(pthread_t));
    if (!threads) {
        fprintf(stderr, "Failed to allocate threads\n");
        return 1;
    }
    
    for (int i = 0; i < num_threads; i++) {
        if (pthread_create(&threads[i], NULL, worker_thread, NULL) != 0) {
            fprintf(stderr, "Failed to create thread %d\n", i);
            return 1;
        }
    }
    
    /* Run for specified duration */
    sleep((unsigned int)duration);
    
    /* Stop workers */
    atomic_store(&g_running, 0);
    
    /* Wait for threads */
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    free(threads);
    
    /* Print results */
    print_results(duration, num_threads);
    
    return 0;
}
