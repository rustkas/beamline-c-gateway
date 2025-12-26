/**
 * bench_memory.c - Memory usage benchmark wrapper
 * 
 * Task 20: Track memory usage under load
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/**
 * Read RSS (Resident Set Size) from /proc/self/status
 */
static long get_rss_kb(void) {
    FILE *f = fopen("/proc/self/status", "r");
    if (!f) return -1;
    
    char line[256];
    long rss = -1;
    
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            sscanf(line + 6, "%ld", &rss);
            break;
        }
    }
    
    fclose(f);
    return rss;
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    printf("Memory Benchmark for IPC Gateway\n");
    printf("=================================\n\n");
    
    printf("This benchmark should be run with memory profiling tools:\n\n");
    
    printf("1. Valgrind (Memory leak detection):\n");
    printf("   valgrind --leak-check=full ./build/ipc-gateway\n\n");
    
    printf("2. Massif (Heap profiling):\n");
    printf("   valgrind --tool=massif ./build/ipc-gateway\n");
    printf("   ms_print massif.out.<pid>\n\n");
    
    printf("3. Heaptrack (Advanced heap profiling):\n");
    printf("   heaptrack ./build/ipc-gateway\n");
    printf("   heaptrack_gui heaptrack.<pid>.gz\n\n");
    
    printf("4. RSS monitoring during load test:\n");
    printf("   ./build/ipc-gateway &\n");
    printf("   watch -n 1 'ps aux | grep ipc-gateway'\n");
    printf("   ./benchmarks/bench_ipc_throughput -d 60\n\n");
    
    /* Also provide a simple RSS tracker */
    printf("Simple RSS Monitor\n");
    printf("------------------\n");
    
    long rss_baseline = get_rss_kb();
    if (rss_baseline > 0) {
        printf("Current RSS: %ld KB (%.1f MB)\n", rss_baseline, (double)rss_baseline / 1024.0);
    } else {
        printf("Failed to read RSS (not Linux?)\n");
    }
    
    printf("\nExpected behavior under load:\n");
    printf("- Baseline RSS: ~10-20 MB\n");
    printf("- Under load RSS: < 50 MB\n");
    printf("- After load RSS: Should return to near baseline (no major leaks)\n");
    
    return 0;
}
