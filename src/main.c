#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <ftdi.h>
#include <libinfnoise.h>
#include "crypto.h"
#include "secp256k1.h"
#include "base58.h"
#include "address_store.h"

static int default_thread_count(void)
{
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return (n > 0) ? (int)n : 4;
}

static void ror_bits(uint8_t *buf, int len, int n)
{
    int total = len * 8;
    n %= total;
    if (n == 0) return;
    int bs = n / 8;
    int br = n % 8;
    uint8_t tmp[len];
    for (int i = 0; i < len; i++) {
        int src = (i + bs) % len;
        tmp[i] = (buf[src] >> br) | (buf[(src + 1) % len] << (8 - br));
    }
    memcpy(buf, tmp, len);
}

static void constrain_private_key(uint8_t key[32], int bitsNum)
{
    if (bitsNum <= 0) return;
    int full_bytes = (bitsNum - 1) / 8;
    int msb_bits = (bitsNum - 1) % 8 + 1;
    int msb_byte = 31 - full_bytes;

    uint8_t mask = (uint8_t)((1u << msb_bits) - 1);
    key[msb_byte] &= mask;
    key[msb_byte] |= (uint8_t)(1u << (msb_bits - 1));

    for (int i = 0; i < msb_byte; i++)
        key[i] = 0;
}

static bool check_address(const uint8_t key[32], const char *target)
{
    uint256 k;
    uint256_from_be_bytes(&k, key);
    ecpoint G = secp256k1_G();
    ecpoint pub = secp256k1_multiply(&k, &G);

    uint8_t pubkey_bytes[33];
    pubkey_bytes[0] = (pub.y.v[0] & 1) ? 0x03 : 0x02;
    uint256_to_be_bytes(&pub.x, pubkey_bytes + 1);

    uint8_t sha256_hash[32];
    SHA256(pubkey_bytes, 33, sha256_hash);

    uint8_t hash160[20];
    RIPEMD160(sha256_hash, 32, hash160);

    char address[64];
    int ret = base58check_encode(hash160, 20, 0x00, address, sizeof(address));
    if (ret < 0) return false;

    return strcmp(address, target) == 0;
}

typedef struct {
    struct infnoise_context *trng;
    pthread_mutex_t *trng_lock;
    int bitsNum;
    const char *target;
    bool *found;
    uint8_t *found_key;
    pthread_mutex_t *found_lock;
    int id;
    bool print_keys;
} WorkerCtx;

#define BATCH_BYTES 5120
#define SEEDS_PER_BATCH (BATCH_BYTES / 32)

static void *worker_routine(void *arg)
{
    WorkerCtx *ctx = (WorkerCtx *)arg;
    uint64_t total_keys = 0;
    printf("[%d] started\n", ctx->id);

    while (!*ctx->found) {
        uint8_t batch[BATCH_BYTES];

        pthread_mutex_lock(ctx->trng_lock);
        #define TRNG_RETRIES 100
        int retries = 0;
        uint32_t got = 0;
        while (got < BATCH_BYTES) {
            ctx->trng->errorFlag = false;
            uint32_t n = readData(ctx->trng, batch + got, true, 0);
            if (ctx->trng->errorFlag) {
                fprintf(stderr, "[%d] readData error: %s\n",
                        ctx->id, ctx->trng->message);
                pthread_mutex_unlock(ctx->trng_lock);
                return NULL;
            }
            if (n == 0) {
                retries++;
                if (retries >= TRNG_RETRIES) {
                    fprintf(stderr, "[%d] TRNG read failed after %d retries\n",
                            ctx->id, TRNG_RETRIES);
                    pthread_mutex_unlock(ctx->trng_lock);
                    return NULL;
                }
                continue;
            }
            retries = 0;
            got += n;
        }
        pthread_mutex_unlock(ctx->trng_lock);

        for (int si = 0; si < SEEDS_PER_BATCH; si++) {
            for (int rot = 0; rot < 256; rot++) {
                if (*ctx->found) return NULL;

                uint8_t key[32];
                memcpy(key, batch + si * 32, 32);
                ror_bits(key, 32, rot);
                constrain_private_key(key, ctx->bitsNum);

                if (ctx->print_keys) {
                    char line[32 * 8 + 50];
                    int pos = sprintf(line, "[%d] ", ctx->id);
                    for (int i = 0; i < 32; i++)
                        for (int b = 7; b >= 0; b--)
                            line[pos++] = (key[i] >> b) & 1 ? '1' : '0';
                    line[pos++] = '\n';
                    fwrite(line, 1, pos, stdout);
                }

                if (check_address(key, ctx->target)) {
                    pthread_mutex_lock(ctx->found_lock);
                    if (!*ctx->found) {
                        *ctx->found = true;
                        memcpy(ctx->found_key, key, 32);
                    }
                    pthread_mutex_unlock(ctx->found_lock);
                    return NULL;
                }

                total_keys++;
                if ((total_keys & 0xFFF) == 0)
                    printf("[%d] %llu\n", ctx->id, (unsigned long long)total_keys);
            }
        }
        printf("[%d] %llu keys checked\n", ctx->id, (unsigned long long)total_keys);
    }
    return NULL;
}

static void print_usage(const char *prog)
{
    printf("Usage: %s [-p] [threads]\n", prog);
    printf("  -p       print every key in binary to stdout\n");
    printf("  threads  number of worker threads (default: CPU count)\n");
}

int main(int argc, char **argv)
{
    bool print_keys = false;
    int num_threads;

    if (argc >= 2 && strcmp(argv[1], "-p") == 0) {
        print_keys = true;
        num_threads = (argc >= 3) ? atoi(argv[2]) : default_thread_count();
    } else if (argc >= 2) {
        num_threads = atoi(argv[1]);
    } else {
        num_threads = default_thread_count();
    }

    if (num_threads <= 0) { print_usage(argv[0]); return 1; }
    printf("Using %d thread(s) (max %d available)%s\n",
           num_threads, default_thread_count(),
           print_keys ? " [printing keys]" : "");

    AddressEntry *entries = NULL;
    int entryCount = 0;
    if (loadAddressEntries("addresses.json", &entries, &entryCount) != 0)
        return 1;

    AddressEntry *entry = &entries[0];
    int bitsNum = entry->bitsNum;
    const char *target_address = entry->address;

    printf("Address: bitsNum=%d target=%s\n", bitsNum, target_address);

    struct infnoise_context trng;
    if (!initInfnoise(&trng, NULL, true, false)) {
        fprintf(stderr, "initInfnoise failed: %s\n", trng.message);
        freeAddressEntries(entries, entryCount);
        return 1;
    }

    bool found = false;
    uint8_t found_key[32];
    pthread_mutex_t trng_lock = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t found_lock = PTHREAD_MUTEX_INITIALIZER;

    pthread_t *threads = malloc(num_threads * sizeof(pthread_t));
    WorkerCtx *wctx = malloc(num_threads * sizeof(WorkerCtx));

    for (int i = 0; i < num_threads; i++) {
        wctx[i].trng       = &trng;
        wctx[i].trng_lock  = &trng_lock;
        wctx[i].bitsNum    = bitsNum;
        wctx[i].target     = target_address;
        wctx[i].found      = &found;
        wctx[i].found_key  = found_key;
        wctx[i].found_lock = &found_lock;
        wctx[i].id         = i;
        wctx[i].print_keys = print_keys;
        pthread_create(&threads[i], NULL, worker_routine, &wctx[i]);
    }

    for (int i = 0; i < num_threads; i++)
        pthread_join(threads[i], NULL);

    free(threads);
    free(wctx);

    if (found) {
        char found_path[256];
        snprintf(found_path, sizeof(found_path), "found/%s", target_address);
        FILE *f = fopen(found_path, "w");
        if (f) {
            for (int b = 0; b < 32; b++)
                fprintf(f, "%02x", found_key[b]);
            fprintf(f, "\n");
            fclose(f);
            printf("FOUND! Private key saved to %s\n", found_path);
            printf("Key: ");
            for (int b = 0; b < 32; b++) printf("%02x", found_key[b]);
            printf("\n");
        } else {
            fprintf(stderr, "Failed to write %s\n", found_path);
        }
    } else {
        printf("Key not found (all workers exited without match)\n");
    }

    deinitInfnoise(&trng);
    freeAddressEntries(entries, entryCount);
    return found ? 0 : 1;
}
