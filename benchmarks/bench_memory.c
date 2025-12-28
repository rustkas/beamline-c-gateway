/**
 * bench_memory.c - IPC Memory usage benchmark
 * 
 * Measures RSS/FD stability under continuous IPC load
 * Uses ipc_protocol.h for framing (single source of truth)
 */

#define _GNU_SOURCE
#include "ipc_protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <errno.h>
#include <arpa/inet.h>
#include <time.h>
#include <inttypes.h>
#include <dirent.h>

#define DEFAULT_SOCKET_PATH "/tmp/beamline-gateway.sock"
#define DEFAULT_REQUESTS 10000
#define SAMPLE_INTERVAL 100
#define IO_TIMEOUT_SEC 10

/* RSS measurement */
static long get_rss_kb(void) {
    FILE *f = fopen("/proc/self/status", "r");
    if (!f) return -1;
    
    char line[256];
    long rss_kb = -1;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            sscanf(line + 6, "%ld", &rss_kb);
            break;
        }
    }
    fclose(f);
    return rss_kb;
}

/* FD count */
static int count_open_fds(void) {
    DIR *d = opendir("/proc/self/fd");
    if (!d) return -1;
    
    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] != '.') count++;
    }
    closedir(d);
    return count - 1; /* exclude /proc/self/fd itself */
}

/* Socket connection */
static int connect_socket(const char *path) {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) return -1;
    
    struct timeval tv = { .tv_sec = IO_TIMEOUT_SEC, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
    
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }
    
    return sock;
}

int main(int argc, char *argv[]) {
    const char *socket_path = DEFAULT_SOCKET_PATH;
    uint32_t requests = DEFAULT_REQUESTS;
    
    /* Parse args - PRIORITY: CLI > ENV > default */
    if (argc > 1) socket_path = argv[1];
    if (argc > 2) requests = (uint32_t)atoi(argv[2]);
    
    /* ENV only if CLI didn't override */
    if (argc <= 1) {
        const char *env = getenv("IPC_SOCKET_PATH");
        if (env && env[0]) socket_path = env;
    }
    
    printf("IPC Memory Benchmark\n");
    printf("socket: %s\n", socket_path);
    printf("requests: %" PRIu32 "\n", requests);
    printf("sample_interval: %d\n\n", SAMPLE_INTERVAL);
    
    /* Baseline */
    long rss_baseline = get_rss_kb();
    int fd_baseline = count_open_fds();
    
    printf("Baseline RSS: %ld KB (%.1f MB)\n", rss_baseline, (double)rss_baseline / 1024.0);
    printf("Baseline FDs: %d\n\n", fd_baseline);
    
    /* Connect */
    int sock = connect_socket(socket_path);
    if (sock < 0) {
        fprintf(stderr, "Failed to connect to %s\n", socket_path);
        return 1;
    }
    
    /* Prepare message using ipc_protocol.h */
    const char *payload_str = "{\"command\":\"memory_test\"}";
    ipc_message_t msg = {
        .type = IPC_MSG_PING,
        .payload = (char*)payload_str,
        .payload_len = strlen(payload_str)
    };
    
    char frame_buf[IPC_MAX_FRAME_SIZE];
    char resp_buf[IPC_MAX_FRAME_SIZE];
    
    long max_rss = rss_baseline;
    int max_fd = fd_baseline;
    
    /* Run requests with sampling */
    for (uint32_t i = 0; i < requests; i++) {
        /* Encode and send using ipc_protocol.h */
        ssize_t frame_len = ipc_encode_message(&msg, frame_buf, sizeof(frame_buf));
        if (frame_len < 0) {
            fprintf(stderr, "Encode failed at %" PRIu32 "\n", i);
            break;
        }
        
        if (send(sock, frame_buf, (size_t)frame_len, MSG_NOSIGNAL) != frame_len) {
            fprintf(stderr, "Send failed at %" PRIu32 "\n", i);
            break;
        }
        
        /* Receive response - read header first */
        char header[IPC_HEADER_SIZE];
        ssize_t n = recv(sock, header, IPC_HEADER_SIZE, 0);
        if (n != IPC_HEADER_SIZE) {
            fprintf(stderr, "Recv header failed at %" PRIu32 "\n", i);
            break;
        }
        
        /* Validate response version and type */
        uint8_t resp_version = (uint8_t)header[4];
        uint8_t resp_type = (uint8_t)header[5];
        
        if (resp_version != IPC_PROTOCOL_VERSION) {
            fprintf(stderr, "Invalid response version at %" PRIu32 ": got 0x%02x, expected 0x%02x\n",
                    i, resp_version, IPC_PROTOCOL_VERSION);
            break;
        }
        
        if (resp_type != IPC_MSG_RESPONSE_OK && resp_type != IPC_MSG_PONG) {
            fprintf(stderr, "Invalid response type at %" PRIu32 ": got 0x%02x\n", i, resp_type);
            break;
        }
        
        /* Parse length */
        uint32_t resp_len;
        memcpy(&resp_len, header, 4);
        resp_len = ntohl(resp_len);
        
        if (resp_len > IPC_HEADER_SIZE && resp_len < IPC_MAX_FRAME_SIZE) {
            size_t payload_len = resp_len - IPC_HEADER_SIZE;
            if (recv(sock, resp_buf, payload_len, 0) != (ssize_t)payload_len) {
                fprintf(stderr, "Recv payload failed at %" PRIu32 "\n", i);
                break;
            }
        }
        
        if (i % SAMPLE_INTERVAL == 0) {
            long rss = get_rss_kb();
            int fds = count_open_fds();
            
            if (rss > max_rss) max_rss = rss;
            if (fds > max_fd) max_fd = fds;
            
            printf("[%6" PRIu32 "] RSS: %6ld KB  FDs: %3d\n", i, rss, fds);
        }
    }
    
    close(sock);
    
    /* Final */
    sleep(1);
    long rss_final = get_rss_kb();
    int fd_final = count_open_fds();
    
    printf("\nSummary:\n");
    printf("  Baseline RSS:  %ld KB (%.1f MB)\n", rss_baseline, (double)rss_baseline / 1024.0);
    printf("  Peak RSS:      %ld KB (%.1f MB)\n", max_rss, (double)max_rss / 1024.0);
    printf("  Final RSS:     %ld KB (%.1f MB)\n", rss_final, (double)rss_final / 1024.0);
    printf("  RSS Growth:    %ld KB (%.1f MB)\n", max_rss - rss_baseline, (double)(max_rss - rss_baseline) / 1024.0);
    printf("\n");
    printf("  Baseline FDs:  %d\n", fd_baseline);
    printf("  Peak FDs:      %d\n", max_fd);
    printf("  Final FDs:     %d\n", fd_final);
    printf("  FD Growth:     %d\n", max_fd - fd_baseline);
    /* Machine-readable JSON output (last line) */
    printf("{\"benchmark\":\"memory\",");
    printf("\"requests\":%u,", requests);
    printf("\"peak_rss_kb\":%ld,", max_rss);
    printf("\"peak_fds\":%d,", max_fd);
    printf("\"final_rss_kb\":%ld,", rss_final);
    printf("\"final_fds\":%d,", fd_final);
    printf("\"exit_code\":0}\\n");

    
    return 0;
}
