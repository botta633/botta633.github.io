// fs_bench.c
// Simple FS workload generator for Brendan Gregg's "record size" experiment.
//
// Build:
//   gcc -O2 fs_bench.c -o fs_bench
//
// Example:
//   ./fs_bench --file data.bin --mode rand \
//              --record-size 4096 \
//              --total-bytes 8589934592 \
//              --seed 123
//
// The program:
//   - Opens the file
//   - Computes ops = total_bytes / record_size
//   - Issues that many reads of size record_size,
//     with offsets chosen in [0, total_bytes - record_size]
//   - Does NO timing and prints NOTHING on success
//   - Errors go to stderr

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

static void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s --file <path> --mode <rand|seq>\n"
        "          --record-size <bytes> --total-bytes <bytes> [--seed <N>]\n",
        prog);
}

int main(int argc, char **argv) {
    const char *file_path = NULL;
    const char *mode = NULL;
    size_t record_size = 0;
    long long total_bytes = 0;
    unsigned int seed = 12345;

    // Simple manual arg parsing
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--file") && i + 1 < argc) {
            file_path = argv[++i];
        } else if (!strcmp(argv[i], "--mode") && i + 1 < argc) {
            mode = argv[++i];
        } else if (!strcmp(argv[i], "--record-size") && i + 1 < argc) {
            record_size = (size_t)strtoull(argv[++i], NULL, 10);
        } else if (!strcmp(argv[i], "--total-bytes") && i + 1 < argc) {
            total_bytes = strtoll(argv[++i], NULL, 10);
        } else if (!strcmp(argv[i], "--seed") && i + 1 < argc) {
            seed = (unsigned int)strtoul(argv[++i], NULL, 10);
        } else {
            fprintf(stderr, "Unknown or incomplete argument: %s\n", argv[i]);
            usage(argv[0]);
            return 1;
        }
    }

    if (!file_path || !mode || record_size == 0 || total_bytes <= 0) {
        fprintf(stderr, "Error: missing required arguments.\n");
        usage(argv[0]);
        return 1;
    }

    int mode_rand = !strcmp(mode, "rand");
    int mode_seq  = !strcmp(mode, "seq");

    if (!(mode_rand || mode_seq)) {
        fprintf(stderr, "Error: invalid mode '%s' (use 'rand' or 'seq')\n", mode);
        usage(argv[0]);
        return 1;
    }

    // Open file
    int fd = open(file_path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "Error: failed to open '%s': %s\n",
                file_path, strerror(errno));
        return 1;
    }

    // Get file size
    struct stat st;
    if (fstat(fd, &st) < 0) {
        fprintf(stderr, "Error: fstat failed on '%s': %s\n",
                file_path, strerror(errno));
        close(fd);
        return 1;
    }
    off_t file_size = st.st_size;
    if (file_size <= 0) {
        fprintf(stderr, "Error: file size is non-positive (%jd)\n",
                (intmax_t)file_size);
        close(fd);
        return 1;
    }

    // Ensure file is big enough for the working set
    if ((long long)file_size < total_bytes) {
        fprintf(stderr,
            "Error: file_size=%jd is smaller than total_bytes=%lld\n",
            (intmax_t)file_size, total_bytes);
        close(fd);
        return 1;
    }

    long long ops = total_bytes / (long long)record_size;
    if (ops <= 0) {
        fprintf(stderr,
            "Error: total_bytes=%lld too small for record_size=%zu\n",
            total_bytes, record_size);
        close(fd);
        return 1;
    }

    // We'll restrict reads to [0, total_bytes - record_size]
    off_t max_offset = (off_t)(total_bytes - (long long)record_size);

    // Seed RNG
    srand(seed);

    // Allocate buffer once
    void *buf = malloc(record_size);
    if (!buf) {
        fprintf(stderr,
                "Error: malloc failed for buf (record_size=%zu)\n",
                record_size);
        close(fd);
        return 1;
    }

    // Workload loop
    for (long long i = 0; i < ops; ++i) {
        off_t offset = 0;

        if (mode_rand) {
            // Choose random offset aligned to record_size in the working set
            long long block_count = (long long)(max_offset / (off_t)record_size) + 1;
            long long block_id = rand() % block_count;
            offset = (off_t)(block_id * (long long)record_size);
        } else { // seq
            long long block_id = i % ((long long)(max_offset / (off_t)record_size) + 1);
            offset = (off_t)(block_id * (long long)record_size);
        }

        ssize_t r = pread(fd, buf, record_size, offset);
        if (r < 0) {
            fprintf(stderr,
                "Error: pread failed (offset=%jd, size=%zu): %s\n",
                (intmax_t)offset, record_size, strerror(errno));
            free(buf);
            close(fd);
            return 1;
        }
        // Short read shouldn't happen here as file_size >= total_bytes
    }

    free(buf);
    close(fd);
    return 0;
}
