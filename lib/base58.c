#include "base58.h"
#include "crypto.h"
#include <string.h>

static const char BASE58_ALPHABET[] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

int base58check_encode(const uint8_t *hash160, size_t hash160_len, uint8_t version, char *output, size_t output_size)
{
    int total_len = (int)(1 + hash160_len + 4);
    uint8_t data[128];
    if ((size_t)total_len > sizeof(data)) return -1;

    data[0] = version;
    memcpy(data + 1, hash160, hash160_len);

    uint8_t hash[32];
    SHA256(data, 1 + hash160_len, hash);
    SHA256(hash, 32, hash);
    memcpy(data + 1 + hash160_len, hash, 4);

    int zeros = 0;
    while (zeros < total_len && data[zeros] == 0) zeros++;

    char buffer[64];
    int buflen = 0;

    int start = zeros;
    while (start < total_len) {
        int remainder = 0;
        for (int i = start; i < total_len; i++) {
            int val = (remainder << 8) | data[i];
            data[i] = (uint8_t)(val / 58);
            remainder = val % 58;
        }
        if (buflen >= (int)sizeof(buffer)) return -1;
        buffer[buflen++] = BASE58_ALPHABET[remainder];
        while (start < total_len && data[start] == 0) start++;
    }

    int outpos = 0;
    for (int i = 0; i < zeros; i++) {
        if ((size_t)outpos >= output_size - 1) return -1;
        output[outpos++] = '1';
    }
    for (int i = buflen - 1; i >= 0; i--) {
        if ((size_t)outpos >= output_size - 1) return -1;
        output[outpos++] = buffer[i];
    }
    output[outpos] = '\0';
    return outpos;
}
