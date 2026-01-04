#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

// 1. Define constants
// A cache line is typically 64 bytes.
// A long is usually 8 bytes.
// So, index 0 and index 1 are in the same line.
// Index 0 and index 16 (16 * 8 = 128 bytes) are definitely in different lines.
#define PADDING_INDEX 16 

typedef struct {
    long *counterToIncrement;
    int id;
} ThreadArgs;

void *incrementCounters(void *arg) {
    ThreadArgs *args = (ThreadArgs *)arg;
    
    // 500 million iterations to make the CPU spin long enough to see results
    long iterations = 500000000; 

    for (long i = 0; i < iterations; i++) {
        // Direct pointer access to shared memory
        (*args->counterToIncrement)++;
    }
    return NULL;
}

int main(int argc, char **argv) {
    int mode = 0; // Default: No False Sharing
    if (argc > 1) {
        mode = atoi(argv[1]);
    }

    // 2. Allocate an array of longs.
    // We allocate enough space for the padded version (approx 20 longs).
    // 'calloc' initializes them to 0.
    long *sharedCounters = calloc(32, sizeof(long));

    printf("----------------------------------------\n");
    if (mode) {
        printf("[!] Mode: FALSE SHARING ENABLED\n");
        printf("    Thread 1 -> Index 0\n");
        printf("    Thread 2 -> Index 1 (Adjacent memory, same cache line)\n");
    } else {
        printf("[:] Mode: False Sharing DISABLED (Optimized)\n");
        printf("    Thread 1 -> Index 0\n");
        printf("    Thread 2 -> Index %d (Far away, different cache line)\n", PADDING_INDEX);
    }
    printf("----------------------------------------\n");

    pthread_t t1, t2;
    ThreadArgs args1, args2;

    // 3. Assign pointers based on mode
    args1.counterToIncrement = &sharedCounters[0];
    args1.id = 1;

    if (mode) {
        // BAD: Right next to counter 0
        args2.counterToIncrement = &sharedCounters[1]; 
    } else {
        // GOOD: Far away from counter 0
        args2.counterToIncrement = &sharedCounters[PADDING_INDEX]; 
    }
    args2.id = 2;

    // Start Timer
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    // Create Threads
    pthread_create(&t1, NULL, incrementCounters, (void *)&args1);
    pthread_create(&t2, NULL, incrementCounters, (void *)&args2);

    // Wait for Threads
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    // Stop Timer
    clock_gettime(CLOCK_MONOTONIC, &end);
    double time_taken = (end.tv_sec - start.tv_sec) + 
                        (end.tv_nsec - start.tv_nsec) / 1e9;

    printf("[*] Finished. Time taken: %.4f seconds\n", time_taken);

    free(sharedCounters);
    return 0;
}
