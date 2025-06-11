#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <x86intrin.h>
#include <openssl/evp.h>
#include <pthread.h>
#include <unistd.h>
#include <math.h>
#include <stdatomic.h>
#include <sys/stat.h>

#define BUFFER_SIZE (1 << 20) 
#define JITTER_SAMPLES 512
#define POOL_SIZE (JITTER_SAMPLES * sizeof(uint64_t))
#define NUM_COLLECTORS 4      
#define TARGET_SIZE (13 * 1024 * 1024)

typedef struct {
    uint8_t buffer[BUFFER_SIZE];
    size_t index;
    pthread_mutex_t lock;
    FILE *output;
    atomic_int done;
    uint64_t histogram[256];

    FILE *source_output;
    pthread_mutex_t source_lock;
} generator_state;

generator_state state = {
    .index = 0,
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .done = 0,
    .histogram = {0},
    .source_output = NULL,
    .source_lock = PTHREAD_MUTEX_INITIALIZER
};

void print_error(const char* msg) {
    fprintf(stderr, "[ERROR] %s\n", msg);
    exit(EXIT_FAILURE);
}

uint64_t highres_clock() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

void jitter_collect(uint8_t *output) {
    static uint64_t last = 0;
    uint64_t current, delta;
    unsigned cycles;

    for(int i = 0; i < JITTER_SAMPLES; i++) {
        current = highres_clock();
        delta = current - __sync_lock_test_and_set(&last, current);

        if(_rdrand32_step(&cycles)) {
            delta ^= cycles;
        }

        __asm__ __volatile__("cpuid" ::: "rax", "rbx", "rcx", "rdx");
        uint64_t tsc = __rdtsc();

        delta = (delta ^ (delta >> 27)) * 0x3C79AC492BA7B653ULL;
        delta += tsc;

        memcpy(output + i*sizeof(delta), &delta, sizeof(delta));
    }
}

void* entropy_collector(void *arg) {
    uint8_t local_buffer[POOL_SIZE];

    while(!atomic_load(&state.done)) {
        jitter_collect(local_buffer);

        pthread_mutex_lock(&state.source_lock);
        if(fwrite(local_buffer, 1, POOL_SIZE, state.source_output) != POOL_SIZE) {
            print_error("Błąd zapisu do source.bin");
        }
        pthread_mutex_unlock(&state.source_lock);

        EVP_MD_CTX *ctx = EVP_MD_CTX_new();
        const EVP_MD *md = EVP_sha3_512();
        unsigned int digest_len;

        EVP_DigestInit_ex(ctx, md, NULL);
        EVP_DigestUpdate(ctx, local_buffer, POOL_SIZE);
        EVP_DigestFinal_ex(ctx, local_buffer, &digest_len);
        EVP_MD_CTX_free(ctx);

        pthread_mutex_lock(&state.lock);
        if(state.index + digest_len <= BUFFER_SIZE) {
            memcpy(state.buffer + state.index, local_buffer, digest_len);
            state.index += digest_len;
        }
        pthread_mutex_unlock(&state.lock);
    }
    return NULL;
}

void* writer_thread(void *arg) {
    const char *filename = "post.bin";
    state.output = fopen(filename, "wb");
    if(!state.output) print_error("Nie można otworzyć pliku wyjściowego");

    size_t total_written = 0;
    struct timespec sleep_time = {0, 1000000};

    while(total_written < TARGET_SIZE) {
        pthread_mutex_lock(&state.lock);
        if(state.index > 0) {
            size_t to_write = (state.index > (TARGET_SIZE - total_written))
                            ? (TARGET_SIZE - total_written)
                            : state.index;

            for(size_t i = 0; i < to_write; i++) {
                state.histogram[state.buffer[i]]++;
            }

            if(fwrite(state.buffer, 1, to_write, state.output) != to_write) {
                print_error("Błąd zapisu do pliku");
            }
            total_written += to_write;

            memmove(state.buffer, state.buffer + to_write, state.index - to_write);
            state.index -= to_write;
        }
        pthread_mutex_unlock(&state.lock);

        nanosleep(&sleep_time, NULL);
    }

    atomic_store(&state.done, 1);
    fclose(state.output);
    return NULL;
}

void calculate_entropy(const char* filename, const char* entropy_file, const char* histogram_file) {
    FILE *file = fopen(filename, "rb");
    if(!file) print_error("Nie można otworzyć pliku do analizy");

    struct stat st;
    if(stat(filename, &st) != 0) print_error("Błąd stat");
    size_t file_size = st.st_size;

    uint64_t histogram[256] = {0};
    uint8_t buffer[BUFFER_SIZE];
    size_t total_read = 0;

    while(total_read < file_size) {
        size_t to_read = (file_size - total_read > BUFFER_SIZE)
                       ? BUFFER_SIZE
                       : file_size - total_read;

        size_t bytes_read = fread(buffer, 1, to_read, file);
        if(bytes_read == 0) break;

        for(size_t i = 0; i < bytes_read; i++) {
            histogram[buffer[i]]++;
        }

        total_read += bytes_read;
    }
    fclose(file);

    double entropy = 0.0;
    for(int i = 0; i < 256; i++) {
        if(histogram[i] > 0) {
            double p = (double)histogram[i]/file_size;
            entropy -= p * log2(p);
        }
    }

    FILE *f = fopen(entropy_file, "w");
    if(f) {
        fprintf(f, "Entropia: %.6f bits/byte\n", entropy);
        fclose(f);
    }

    f = fopen(histogram_file, "w");
    if(f) {
        for(int i = 0; i < 256; i++) {
            fprintf(f, "%d %lu\n", i, histogram[i]);
        }
        fclose(f);
    }
}

int main() {
    state.source_output = fopen("source.bin", "wb");
    if(!state.source_output) print_error("Nie można otworzyć pliku źródłowego");

    pthread_t collectors[NUM_COLLECTORS], writer;

    for(int i = 0; i < NUM_COLLECTORS; i++) {
        if(pthread_create(&collectors[i], NULL, entropy_collector, NULL)) {
            print_error("Błąd tworzenia wątku zbierającego");
        }
    }

    if(pthread_create(&writer, NULL, writer_thread, NULL)) {
        print_error("Błąd tworzenia wątku zapisującego");
    }

    for(int i = 0; i < NUM_COLLECTORS; i++) {
        pthread_join(collectors[i], NULL);
    }
    pthread_join(writer, NULL);

    fclose(state.source_output);

    calculate_entropy("post.bin", "post_entropy.txt", "post_histogram.txt");

    calculate_entropy("source.bin", "source_entropy.txt", "source_histogram.txt");

    printf("[SUKCES] Analiza zakończona\n");
    return 0;
}
