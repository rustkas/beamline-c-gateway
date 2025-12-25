/**
 * fuzz_ipc_protocol.c - Fuzz test for IPC protocol parser
 * 
 * Tests protocol decoder with random/malformed input
 */

#include "ipc_protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Simple pseudo-random generator */
static unsigned int fuzz_seed = 0;

static unsigned int fuzz_rand(void) {
    fuzz_seed = fuzz_seed * 1103515245 + 12345;
    return fuzz_seed;
}

/* Generate random frame */
static void generate_random_frame(uint8_t *buf, size_t size) {
    for (size_t i = 0; i < size; i++) {
        buf[i] = (uint8_t)(fuzz_rand() % 256);
    }
}

/* Test protocol decoder with random input */
static void fuzz_decoder(int iterations) {
    uint8_t frame[1024];
    ipc_message_t msg;
    
    printf("Fuzzing protocol decoder (%d iterations)...\n", iterations);
    
    for (int i = 0; i < iterations; i++) {
        /* Generate random frame */
        size_t frame_size = (fuzz_rand() % sizeof(frame)) + 1;
        generate_random_frame(frame, frame_size);
        
        /* Try to decode (should not crash) */
        ipc_error_t err = ipc_decode_message(frame, frame_size, &msg);
        
        /* If successful, free payload */
        if (err == IPC_ERR_OK) {
            ipc_free_message(&msg);
        }
        
        /* Progress */
        if ((i + 1) % 1000 == 0) {
            printf("  %d iterations... OK\n", i + 1);
        }
    }
    
    printf("Fuzzing complete: %d iterations, no crashes!\n", iterations);
}

/* Test with malformed frames */
static void fuzz_edge_cases(void) {
    printf("Testing edge cases...\n");
    
    ipc_message_t msg;
    uint8_t frame[1024];
    
    /* Empty frame */
    ipc_decode_message(frame, 0, &msg);
    
    /* Too small */
    ipc_decode_message(frame, 1, &msg);
    
    /* Header only */
    ipc_decode_message(frame, IPC_HEADER_SIZE, &msg);
    
    /* Huge length claim */
    memset(frame, 0xFF, sizeof(frame));
    ipc_decode_message(frame, sizeof(frame), &msg);
    
    /* Invalid version */
    memset(frame, 0, sizeof(frame));
    frame[4] = 0xFF;  /* version */
    ipc_decode_message(frame, 100, &msg);
    
    printf("Edge cases: OK\n");
}

int main(int argc, char **argv) {
    printf("=== IPC Protocol Fuzz Test ===\n");
    
    /* Seed from time */
    fuzz_seed = (unsigned int)time(NULL);
    
    int iterations = 10000;  /* Default */
    if (argc > 1) {
        iterations = atoi(argv[1]);
    }
    
    fuzz_edge_cases();
    fuzz_decoder(iterations);
    
    printf("\n✅ All fuzz tests passed - no crashes!\n");
    return 0;
}
